/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include "inc/Main.h"
#include "inc/debug.h"
#include "inc/Endian.h"
#include "inc/Pass.h"
#include <cstring>
#include <cstdlib>
#include <cassert>
#include "inc/Segment.h"
#include "inc/Code.h"
#include "inc/Rule.h"

using namespace graphite2;
using vm::Machine;
typedef Machine::Code  Code;


Pass::Pass()
: m_silf(0),
  m_cols(0),
  m_rules(0),
  m_ruleMap(0),
  m_startStates(0),
  m_transitions(0),
  m_states(0),
  m_flags(0),
  m_iMaxLoop(0),
  m_numGlyphs(0),
  m_numRules(0),
  m_numStates(0),
  m_numTransition(0),
  m_numSuccess(0),
  m_numColumns(0),
  m_minPreCtxt(0),
  m_maxPreCtxt(0)
{
}

Pass::~Pass()
{
    free(m_cols);
    free(m_startStates);
    free(m_transitions);
    free(m_states);
    free(m_ruleMap);

    delete [] m_rules;
}

bool Pass::readPass(const byte * const pass_start, size_t pass_length, size_t subtable_base, GR_MAYBE_UNUSED const Face & face)
{
    const byte *                p = pass_start,
               * const pass_end   = p + pass_length;
    size_t numRanges;

    if (pass_length < 40) return false; 
    // Read in basic values
    m_flags = be::read<byte>(p);
    m_iMaxLoop = be::read<byte>(p);
    be::skip<byte>(p,2); // skip maxContext & maxBackup
    m_numRules = be::read<uint16>(p);
    be::skip<uint16>(p);   // fsmOffset - not sure why we would want this
    const byte * const pcCode = pass_start + be::read<uint32>(p) - subtable_base,
               * const rcCode = pass_start + be::read<uint32>(p) - subtable_base,
               * const aCode  = pass_start + be::read<uint32>(p) - subtable_base;
    be::skip<uint32>(p);
    m_numStates = be::read<uint16>(p);
    m_numTransition = be::read<uint16>(p);
    m_numSuccess = be::read<uint16>(p);
    m_numColumns = be::read<uint16>(p);
    numRanges = be::read<uint16>(p);
    be::skip<uint16>(p, 3); // skip searchRange, entrySelector & rangeShift.
    assert(p - pass_start == 40);
    // Perform some sanity checks.
    if (   m_numTransition > m_numStates
            || m_numSuccess > m_numStates
            || m_numSuccess + m_numTransition < m_numStates
            || numRanges == 0)
        return false;

    m_successStart = m_numStates - m_numSuccess;
    if (p + numRanges * 6 - 4 > pass_end) return false;
    m_numGlyphs = be::peek<uint16>(p + numRanges * 6 - 4) + 1;
    // Calculate the start of various arrays.
    const byte * const ranges = p;
    be::skip<uint16>(p, numRanges*3);
    const byte * const o_rule_map = p;
    be::skip<uint16>(p, m_numSuccess + 1);

    // More sanity checks
    if (reinterpret_cast<const byte *>(o_rule_map + m_numSuccess*sizeof(uint16)) > pass_end
            || p > pass_end)
        return false;
    const size_t numEntries = be::peek<uint16>(o_rule_map + m_numSuccess*sizeof(uint16));
    const byte * const   rule_map = p;
    be::skip<uint16>(p, numEntries);

    if (p + 2*sizeof(uint8) > pass_end) return false;
    m_minPreCtxt = be::read<uint8>(p);
    m_maxPreCtxt = be::read<uint8>(p);
    if (m_minPreCtxt > m_maxPreCtxt) return false;
    const byte * const start_states = p;
    be::skip<int16>(p, m_maxPreCtxt - m_minPreCtxt + 1);
    const uint16 * const sort_keys = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules);
    const byte * const precontext = p;
    be::skip<byte>(p, m_numRules);
    be::skip<byte>(p);     // skip reserved byte

    if (p + sizeof(uint16) > pass_end) return false;
    const size_t pass_constraint_len = be::read<uint16>(p);
    const uint16 * const o_constraint = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules + 1);
    const uint16 * const o_actions = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules + 1);
    const byte * const states = p;
    be::skip<int16>(p, m_numTransition*m_numColumns);
    be::skip<byte>(p);          // skip reserved byte
    if (p != pcCode || p >= pass_end) return false;
    be::skip<byte>(p, pass_constraint_len);
    if (p != rcCode || p >= pass_end
        || size_t(rcCode - pcCode) != pass_constraint_len) return false;
    be::skip<byte>(p, be::peek<uint16>(o_constraint + m_numRules));
    if (p != aCode || p >= pass_end) return false;
    be::skip<byte>(p, be::peek<uint16>(o_actions + m_numRules));

    // We should be at the end or within the pass
    if (p > pass_end) return false;

    // Load the pass constraint if there is one.
    if (pass_constraint_len)
    {
        m_cPConstraint = vm::Machine::Code(true, pcCode, pcCode + pass_constraint_len, 
                                  precontext[0], be::peek<uint16>(sort_keys), *m_silf, face);
        if (!m_cPConstraint) return false;
    }
    if (!readRanges(ranges, numRanges)) return false;
    if (!readRules(rule_map, numEntries,  precontext, sort_keys,
                   o_constraint, rcCode, o_actions, aCode, face)) return false;
#ifdef GRAPHITE2_TELEMETRY
    telemetry::category _states_cat(face.tele.states);
#endif
    return readStates(start_states, states, o_rule_map, face);
}


bool Pass::readRules(const byte * rule_map, const size_t num_entries,
                     const byte *precontext, const uint16 * sort_key,
                     const uint16 * o_constraint, const byte *rc_data,
                     const uint16 * o_action,     const byte * ac_data,
                     const Face & face)
{
    const byte * const ac_data_end = ac_data + be::peek<uint16>(o_action + m_numRules);
    const byte * const rc_data_end = rc_data + be::peek<uint16>(o_constraint + m_numRules);

    if (!(m_rules = new Rule [m_numRules])) return false;
    precontext += m_numRules;
    sort_key   += m_numRules;
    o_constraint += m_numRules;
    o_action += m_numRules;

    // Load rules.
    const byte * ac_begin = 0, * rc_begin = 0,
               * ac_end = ac_data + be::peek<uint16>(o_action),
               * rc_end = rc_data + be::peek<uint16>(o_constraint);
    Rule * r = m_rules + m_numRules - 1;
    for (size_t n = m_numRules; n; --n, --r, ac_end = ac_begin, rc_end = rc_begin)
    {
        r->preContext = *--precontext;
        r->sort       = be::peek<uint16>(--sort_key);
#ifndef NDEBUG
        r->rule_idx   = n - 1;
#endif
        if (r->sort > 63 || r->preContext >= r->sort || r->preContext > m_maxPreCtxt || r->preContext < m_minPreCtxt)
            return false;
        ac_begin      = ac_data + be::peek<uint16>(--o_action);
        --o_constraint;
        rc_begin      = be::peek<uint16>(o_constraint) ? rc_data + be::peek<uint16>(o_constraint) : rc_end;

        if (ac_begin > ac_end || ac_begin > ac_data_end || ac_end > ac_data_end
                || rc_begin > rc_end || rc_begin > rc_data_end || rc_end > rc_data_end)
            return false;
        r->action     = new vm::Machine::Code(false, ac_begin, ac_end, r->preContext, r->sort, *m_silf, face);
        r->constraint = new vm::Machine::Code(true,  rc_begin, rc_end, r->preContext, r->sort, *m_silf, face);

        if (!r->action || !r->constraint
                || r->action->status() != Code::loaded
                || r->constraint->status() != Code::loaded
                || !r->constraint->immutable())
            return false;
    }

    // Load the rule entries map
    RuleEntry * re = m_ruleMap = gralloc<RuleEntry>(num_entries);
    for (size_t n = num_entries; n; --n, ++re)
    {
        const ptrdiff_t rn = be::read<uint16>(rule_map);
        if (rn >= m_numRules)  return false;
        re->rule = m_rules + rn;
    }

    return true;
}

static int cmpRuleEntry(const void *a, const void *b) { return (*(RuleEntry *)a < *(RuleEntry *)b ? -1 :
                                                                (*(RuleEntry *)b < *(RuleEntry *)a ? 1 : 0)); }

bool Pass::readStates(const byte * starts, const byte *states, const byte * o_rule_map, GR_MAYBE_UNUSED const Face & face)
{
#ifdef GRAPHITE2_TELEMETRY
    telemetry::category _states_cat(face.tele.starts);
#endif
    m_startStates = gralloc<uint16>(m_maxPreCtxt - m_minPreCtxt + 1);
#ifdef GRAPHITE2_TELEMETRY
    telemetry::set_category(face.tele.states);
#endif
    m_states      = gralloc<State>(m_numStates);
#ifdef GRAPHITE2_TELEMETRY
    telemetry::set_category(face.tele.transitions);
#endif
    m_transitions      = gralloc<uint16>(m_numTransition * m_numColumns);

    if (!m_startStates || !m_states || !m_transitions) return false;
    // load start states
    for (uint16 * s = m_startStates,
                * const s_end = s + m_maxPreCtxt - m_minPreCtxt + 1; s != s_end; ++s)
    {
        *s = be::read<uint16>(starts);
        if (*s >= m_numStates) return false; // true;
    }

    // load state transition table.
    for (uint16 * t = m_transitions,
                * const t_end = t + m_numTransition*m_numColumns; t != t_end; ++t)
    {
        *t = be::read<uint16>(states);
        if (*t >= m_numStates) return false;
    }

    State * s = m_states,
          * const success_begin = m_states + m_numStates - m_numSuccess;
    const RuleEntry * rule_map_end = m_ruleMap + be::peek<uint16>(o_rule_map + m_numSuccess*sizeof(uint16));
    for (size_t n = m_numStates; n; --n, ++s)
    {
        RuleEntry * const begin = s < success_begin ? 0 : m_ruleMap + be::read<uint16>(o_rule_map),
                  * const end   = s < success_begin ? 0 : m_ruleMap + be::peek<uint16>(o_rule_map);

        if (begin >= rule_map_end || end > rule_map_end || begin > end)
            return false;
        s->rules = begin;
        s->rules_end = (end - begin <= FiniteStateMachine::MAX_RULES)? end :
            begin + FiniteStateMachine::MAX_RULES;
        qsort(begin, end - begin, sizeof(RuleEntry), &cmpRuleEntry);
    }

    return true;
}

bool Pass::readRanges(const byte * ranges, size_t num_ranges)
{
    m_cols = gralloc<uint16>(m_numGlyphs);
    memset(m_cols, 0xFF, m_numGlyphs * sizeof(uint16));
    for (size_t n = num_ranges; n; --n)
    {
        uint16     * ci     = m_cols + be::read<uint16>(ranges),
                   * ci_end = m_cols + be::read<uint16>(ranges) + 1,
                     col    = be::read<uint16>(ranges);

        if (ci >= ci_end || ci_end > m_cols+m_numGlyphs || col >= m_numColumns)
            return false;

        // A glyph must only belong to one column at a time
        while (ci != ci_end && *ci == 0xffff)
            *ci++ = col;

        if (ci != ci_end)
            return false;
    }
    return true;
}


void Pass::runGraphite(Machine & m, FiniteStateMachine & fsm) const
{
	Slot *s = m.slotMap().segment.first();
	if (!s || !testPassConstraint(m)) return;
    Slot *currHigh = s->next();

#if !defined GRAPHITE2_NTRACING
    if (fsm.dbgout)  *fsm.dbgout << "rules"	<< json::array;
	json::closer rules_array_closer(fsm.dbgout);
#endif

    m.slotMap().highwater(currHigh);
    int lc = m_iMaxLoop;
    do
    {
        findNDoRule(s, m, fsm);
        if (s && (m.slotMap().highpassed() || s == m.slotMap().highwater() || --lc == 0)) {
        	if (!lc)
        	{
//        		if (dbgout)	*dbgout << json::item << json::flat << rule_event(-1, s, 1);
        		s = m.slotMap().highwater();
        	}
        	lc = m_iMaxLoop;
            if (s)
            	m.slotMap().highwater(s->next());
        }
    } while (s);
}

bool Pass::runFSM(FiniteStateMachine& fsm, Slot * slot) const
{
	fsm.reset(slot, m_maxPreCtxt);
    if (fsm.slots.context() < m_minPreCtxt)
        return false;

    uint16 state = m_startStates[m_maxPreCtxt - fsm.slots.context()];
    uint8  free_slots = SlotMap::MAX_SLOTS;
    do
    {
        fsm.slots.pushSlot(slot);
        if (--free_slots == 0
         || slot->gid() >= m_numGlyphs
         || m_cols[slot->gid()] == 0xffffU
         || state >= m_numTransition)
            return free_slots != 0;

        const uint16 * transitions = m_transitions + state*m_numColumns;
        state = transitions[m_cols[slot->gid()]];
        if (state >= m_successStart)
            fsm.rules.accumulate_rules(m_states[state]);

        slot = slot->next();
    } while (state != 0 && slot);

    fsm.slots.pushSlot(slot);
    return true;
}

#if !defined GRAPHITE2_NTRACING

inline
Slot * input_slot(const SlotMap &  slots, const int n)
{
	Slot * s = slots[slots.context() + n];
	if (!s->isCopied()) 	return s;

	return s->prev() ? s->prev()->next() : (s->next() ? s->next()->prev() : slots.segment.last());
}

inline
Slot * output_slot(const SlotMap &  slots, const int n)
{
	Slot * s = slots[slots.context() + n - 1];
	return s ? s->next() : slots.segment.first();
}

#endif //!defined GRAPHITE2_NTRACING

void Pass::findNDoRule(Slot * & slot, Machine &m, FiniteStateMachine & fsm) const
{
    assert(slot);

    if (runFSM(fsm, slot))
    {
        // Search for the first rule which passes the constraint
        const RuleEntry *        r = fsm.rules.begin(),
                        * const re = fsm.rules.end();
        while (r != re && !testConstraint(*r->rule, m)) ++r;

#if !defined GRAPHITE2_NTRACING
        if (fsm.dbgout)
        {
        	if (fsm.rules.size() != 0)
        	{
				*fsm.dbgout << json::item << json::object;
				dumpRuleEventConsidered(fsm, *r);
				if (r != re)
				{
					const int adv = doAction(r->rule->action, slot, m);
					dumpRuleEventOutput(fsm, *r->rule, slot);
					if (r->rule->action->deletes()) fsm.slots.collectGarbage();
					adjustSlot(adv, slot, fsm.slots);
					*fsm.dbgout	<< "cursor" << objectid(dslot(&fsm.slots.segment, slot))
							<< json::close; // Close RuelEvent object

					return;
				}
				else
				{
					*fsm.dbgout << json::close	// close "considered" array
							<< "output" << json::null
							<< "cursor"	<< objectid(dslot(&fsm.slots.segment, slot->next()))
							<< json::close;
				}
        	}
        }
        else
#endif
        {
   	        if (r != re)
			{
				const int adv = doAction(r->rule->action, slot, m);
				if (r->rule->action->deletes()) fsm.slots.collectGarbage();
				adjustSlot(adv, slot, fsm.slots);
				return;
			}
        }
    }

    slot = slot->next();
}

#if !defined GRAPHITE2_NTRACING

void Pass::dumpRuleEventConsidered(const FiniteStateMachine & fsm, const RuleEntry & re) const
{
	*fsm.dbgout << "considered" << json::array;
	for (const RuleEntry *r = fsm.rules.begin(); r != &re; ++r)
	{
		if (r->rule->preContext > fsm.slots.context())	continue;
	*fsm.dbgout << json::flat << json::object
					<< "id" 	<< r->rule - m_rules
					<< "failed"	<< true
					<< "input" << json::flat << json::object
						<< "start" << objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, -r->rule->preContext)))
						<< "length" << r->rule->sort
						<< json::close	// close "input"
					<< json::close;	// close Rule object
	}
}


void Pass::dumpRuleEventOutput(const FiniteStateMachine & fsm, const Rule & r, Slot * const last_slot) const
{
	*fsm.dbgout		<< json::item << json::flat << json::object
						<< "id" 	<< &r - m_rules
						<< "failed" << false
						<< "input" << json::flat << json::object
							<< "start" << objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, 0)))
							<< "length" << r.sort - r.preContext
							<< json::close // close "input"
						<< json::close	// close Rule object
				<< json::close // close considered array
				<< "output" << json::object
					<< "range" << json::flat << json::object
						<< "start"	<< objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, 0)))
						<< "end"	<< objectid(dslot(&fsm.slots.segment, last_slot))
					<< json::close // close "input"
					<< "slots"	<< json::array;
	const Position rsb_prepos = last_slot ? last_slot->origin() : fsm.slots.segment.advance();
	fsm.slots.segment.positionSlots(0);

	for(Slot * slot = output_slot(fsm.slots, 0); slot != last_slot; slot = slot->next())
		*fsm.dbgout		<< dslot(&fsm.slots.segment, slot);
	*fsm.dbgout			<< json::close 	// close "slots"
					<< "postshift"	<< (last_slot ? last_slot->origin() : fsm.slots.segment.advance()) - rsb_prepos
				<< json::close;			// close "output" object

}

#endif


inline
bool Pass::testPassConstraint(Machine & m) const
{
    if (!m_cPConstraint) return true;

    assert(m_cPConstraint.constraint());

    m.slotMap().reset(*m.slotMap().segment.first(), 0);
    m.slotMap().pushSlot(m.slotMap().segment.first());
    vm::slotref * map = m.slotMap().begin();
    const uint32 ret = m_cPConstraint.run(m, map);

#if !defined GRAPHITE2_NTRACING
    json * const dbgout = m.slotMap().segment.getFace()->logger();
    if (dbgout)
    	*dbgout << "constraint" << (ret && m.status() == Machine::finished);
#endif

    return ret && m.status() == Machine::finished;
}


bool Pass::testConstraint(const Rule & r, Machine & m) const
{
	const uint16 curr_context = m.slotMap().context();
    if (unsigned(r.sort - r.preContext) > m.slotMap().size() - curr_context
    	|| curr_context - r.preContext < 0) return false;
    if (!*r.constraint) return true;
    assert(r.constraint->constraint());

    vm::slotref * map = m.slotMap().begin() + curr_context - r.preContext;
    for (int n = r.sort; n && map; --n, ++map)
    {
    	if (!*map) continue;
        const int32 ret = r.constraint->run(m, map);
        if (!ret || m.status() != Machine::finished)
            return false;
    }

    return true;
}


void SlotMap::collectGarbage()
{
    for(Slot **s = begin(), *const *const se = end() - 1; s != se; ++s) {
        Slot *& slot = *s;
        if(slot->isDeleted() || slot->isCopied())
            segment.freeSlot(slot);
    }
}



int Pass::doAction(const Code *codeptr, Slot * & slot_out, vm::Machine & m) const
{
    assert(codeptr);
    if (!*codeptr) return 0;
    SlotMap   & smap = m.slotMap();
    vm::slotref * map = &smap[smap.context()];
    smap.highpassed(false);

    int32 ret = codeptr->run(m, map);

    if (m.status() != Machine::finished)
    {
    	slot_out = NULL;
    	smap.highwater(0);
    	return 0;
    }

    slot_out = *map;
    return ret;
}


void Pass::adjustSlot(int delta, Slot * & slot_out, SlotMap & smap) const
{
    if (delta < 0)
    {
        if (!slot_out)
        {
            slot_out = smap.segment.last();
            ++delta;
            if (smap.highpassed() && !smap.highwater())
            	smap.highpassed(false);
        }
        while (++delta <= 0 && slot_out)
        {
            if (smap.highpassed() && smap.highwater() == slot_out)
            	smap.highpassed(false);
            slot_out = slot_out->prev();
        }
    }
    else if (delta > 0)
    {
        if (!slot_out)
        {
            slot_out = smap.segment.first();
            --delta;
        }
        while (--delta >= 0 && slot_out)
        {
            slot_out = slot_out->next();
            if (slot_out == smap.highwater() && slot_out)
                smap.highpassed(true);
        }
    }
}

