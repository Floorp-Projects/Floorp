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
    suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include "inc/Main.h"
#include "inc/Slot.h"
#include "inc/Segment.h"

using namespace graphite2;

enum DirCode {  // Hungarian: dirc
        Unk        = -1,
        N          =  0,   // other neutrals (default) - ON
        L          =  1,   // left-to-right, strong - L
        R          =  2,   // right-to-left, strong - R
        AL         =  3,   // Arabic letter, right-to-left, strong, AR
        EN         =  4,   // European number, left-to-right, weak - EN
        EUS        =  5,   // European separator, left-to-right, weak - ES
        ET         =  6,   // European number terminator, left-to-right, weak - ET
        AN         =  7,   // Arabic number, left-to-right, weak - AN
        CUS        =  8,   // Common number separator, left-to-right, weak - CS
        WS         =  9,   // white space, neutral - WS
        BN         = 10,   // boundary neutral - BN

        LRO        = 11,   // LTR override
        RLO        = 12,   // RTL override
        LRE        = 13,   // LTR embedding
        RLE        = 14,   // RTL embedding
        PDF        = 15,   // pop directional format
        NSM        = 16,   // non-space mark

        ON = N
};

enum DirMask {
    Nmask = 1,
    Lmask = 2,
    Rmask = 4,
    ALmask = 8,
    ENmask = 0x10,
    ESmask = 0x20,
    ETmask = 0x40,
    ANmask = 0x80,
    CSmask = 0x100,
    WSmask = 0x200,
    BNmask = 0x400,
    LROmask = 0x800,
    RLOmask = 0x1000,
    LREmask = 0x2000,
    RLEmask = 0x4000,
    PDFmask = 0x8000,
    NSMmask = 0x10000
};

unsigned int bidi_class_map[] = { 0, 1, 2, 5, 4, 8, 9, 3, 7, 0, 0, 0, 0, 0, 0, 0, 6 };
// Algorithms based on Unicode reference standard code. Thanks Asmus Freitag.
#define MAX_LEVEL 61

Slot *resolveExplicit(int level, int dir, Slot *s, int nNest = 0)
{
    int nLastValid = nNest;
    Slot *res = NULL;
    for ( ; s && !res; s = s->next())
    {
        int cls = s->getBidiClass();
        switch(cls)
        {
        case LRO:
        case LRE:
            nNest++;
            if (level & 1)
                s->setBidiLevel(level + 1);
            else
                s->setBidiLevel(level + 2);
            if (s->getBidiLevel() > MAX_LEVEL)
                s->setBidiLevel(level);
            else
            {
                s = resolveExplicit(s->getBidiLevel(), (cls == LRE ? N : L), s->next(), nNest);
                nNest--;
                if (s) continue; else break;
            }
            cls = BN;
            s->setBidiClass(cls);
            break;

        case RLO:
        case RLE:
            nNest++;
            if (level & 1)
                s->setBidiLevel(level + 2);
            else
                s->setBidiLevel(level + 1);
            if (s->getBidiLevel() > MAX_LEVEL)
                s->setBidiLevel(level);
            else
            {
                s = resolveExplicit(s->getBidiLevel(), (cls == RLE ? N : R), s->next(), nNest);
                nNest--;
                if (s) continue; else break;
            }
            cls = BN;
            s->setBidiClass(cls);
            break;

        case PDF:
            cls = BN;
            s->setBidiClass(cls);
            if (nNest)
            {
                if (nLastValid < nNest)
                    --nNest;
                else
                    res = s;
            }
            break;
        }

        if (dir != N)
            cls = dir;
        if (s)
        {
            s->setBidiLevel(level);
            if (s->getBidiClass() != BN)
                s->setBidiClass(cls);
        }
        else
            break;
    }
    return res;
}

// === RESOLVE WEAK TYPES ================================================

enum bidi_state // possible states
{
        xa,             //      arabic letter
        xr,             //      right leter
        xl,             //      left letter

        ao,             //      arabic lett. foll by ON
        ro,             //      right lett. foll by ON
        lo,             //      left lett. foll by ON

        rt,             //      ET following R
        lt,             //      ET following L

        cn,             //      EN, AN following AL
        ra,             //      arabic number foll R
        re,             //      european number foll R
        la,             //      arabic number foll L
        le,             //      european number foll L

        ac,             //      CS following cn
        rc,             //      CS following ra
        rs,             //      CS,ES following re
        lc,             //      CS following la
        ls,             //      CS,ES following le

        ret,            //      ET following re
        let,            //      ET following le
} ;

enum bidi_state_mask
{
    xamask = 1,
    xrmask = 2,
    xlmask = 4,
    aomask = 8,
    romask = 0x10,
    lomask = 0x20,
    rtmask = 0x40,
    ltmask = 0x80,
    cnmask = 0x100,
    ramask = 0x200,
    remask = 0x400,
    lamask = 0x800,
    lemask = 0x1000,
    acmask = 0x2000,
    rcmask = 0x4000,
    rsmask = 0x8000,
    lcmask = 0x10000,
    lsmask = 0x20000,
    retmask = 0x40000,
    letmask = 0x80000
};

const bidi_state stateWeak[][10] =
{
        //      N,  L,  R,  AN, EN, AL,NSM, CS, ES, ET,
{ /*xa*/        ao, xl, xr, cn, cn, xa, xa, ao, ao, ao, /* arabic letter          */ },
{ /*xr*/        ro, xl, xr, ra, re, xa, xr, ro, ro, rt, /* right letter           */ },
{ /*xl*/        lo, xl, xr, la, le, xa, xl, lo, lo, lt, /* left letter            */ },

{ /*ao*/        ao, xl, xr, cn, cn, xa, ao, ao, ao, ao, /* arabic lett. foll by ON*/ },
{ /*ro*/        ro, xl, xr, ra, re, xa, ro, ro, ro, rt, /* right lett. foll by ON */ },
{ /*lo*/        lo, xl, xr, la, le, xa, lo, lo, lo, lt, /* left lett. foll by ON  */ },

{ /*rt*/        ro, xl, xr, ra, re, xa, rt, ro, ro, rt, /* ET following R         */ },
{ /*lt*/        lo, xl, xr, la, le, xa, lt, lo, lo, lt, /* ET following L         */ },

{ /*cn*/        ao, xl, xr, cn, cn, xa, cn, ac, ao, ao, /* EN, AN following AL    */ },
{ /*ra*/        ro, xl, xr, ra, re, xa, ra, rc, ro, rt, /* arabic number foll R   */ },
{ /*re*/        ro, xl, xr, ra, re, xa, re, rs, rs,ret, /* european number foll R */ },
{ /*la*/        lo, xl, xr, la, le, xa, la, lc, lo, lt, /* arabic number foll L   */ },
{ /*le*/        lo, xl, xr, la, le, xa, le, ls, ls,let, /* european number foll L */ },

{ /*ac*/        ao, xl, xr, cn, cn, xa, ao, ao, ao, ao, /* CS following cn        */ },
{ /*rc*/        ro, xl, xr, ra, re, xa, ro, ro, ro, rt, /* CS following ra        */ },
{ /*rs*/        ro, xl, xr, ra, re, xa, ro, ro, ro, rt, /* CS,ES following re     */ },
{ /*lc*/        lo, xl, xr, la, le, xa, lo, lo, lo, lt, /* CS following la        */ },
{ /*ls*/        lo, xl, xr, la, le, xa, lo, lo, lo, lt, /* CS,ES following le     */ },

{ /*ret*/ ro, xl, xr, ra, re, xa,ret, ro, ro,ret, /* ET following re              */ },
{ /*let*/ lo, xl, xr, la, le, xa,let, lo, lo,let, /* ET following le              */ },


};

enum bidi_action // possible actions
{
        // primitives
        IX = 0x100,                     // increment
        XX = 0xF,                       // no-op

        // actions
        xxx = (XX << 4) + XX,           // no-op
        xIx = IX + xxx,                         // increment run
        xxN = (XX << 4) + ON,           // set current to N
        xxE = (XX << 4) + EN,           // set current to EN
        xxA = (XX << 4) + AN,           // set current to AN
        xxR = (XX << 4) + R,            // set current to R
        xxL = (XX << 4) + L,            // set current to L
        Nxx = (ON << 4) + 0xF,          // set run to neutral
        Axx = (AN << 4) + 0xF,          // set run to AN
        ExE = (EN << 4) + EN,           // set run to EN, set current to EN
        NIx = (ON << 4) + 0xF + IX,     // set run to N, increment
        NxN = (ON << 4) + ON,           // set run to N, set current to N
        NxR = (ON << 4) + R,            // set run to N, set current to R
        NxE = (ON << 4) + EN,           // set run to N, set current to EN

        AxA = (AN << 4) + AN,           // set run to AN, set current to AN
        NxL = (ON << 4) + L,            // set run to N, set current to L
        LxL = (L << 4) + L,             // set run to L, set current to L
};


const bidi_action actionWeak[][10] =
{
          //   N,.. L,   R,  AN,  EN,  AL, NSM,  CS,..ES,  ET,
{ /*xa*/ xxx, xxx, xxx, xxx, xxA, xxR, xxR, xxN, xxN, xxN, /* arabic letter             */ },
{ /*xr*/ xxx, xxx, xxx, xxx, xxE, xxR, xxR, xxN, xxN, xIx, /* right leter               */ },
{ /*xl*/ xxx, xxx, xxx, xxx, xxL, xxR, xxL, xxN, xxN, xIx, /* left letter               */ },

{ /*ao*/ xxx, xxx, xxx, xxx, xxA, xxR, xxN, xxN, xxN, xxN, /* arabic lett. foll by ON   */ },
{ /*ro*/ xxx, xxx, xxx, xxx, xxE, xxR, xxN, xxN, xxN, xIx, /* right lett. foll by ON    */ },
{ /*lo*/ xxx, xxx, xxx, xxx, xxL, xxR, xxN, xxN, xxN, xIx, /* left lett. foll by ON     */ },

{ /*rt*/ Nxx, Nxx, Nxx, Nxx, ExE, NxR, xIx, NxN, NxN, xIx, /* ET following R            */ },
{ /*lt*/ Nxx, Nxx, Nxx, Nxx, LxL, NxR, xIx, NxN, NxN, xIx, /* ET following L            */ },

{ /*cn*/ xxx, xxx, xxx, xxx, xxA, xxR, xxA, xIx, xxN, xxN, /* EN, AN following  AL      */ },
{ /*ra*/ xxx, xxx, xxx, xxx, xxE, xxR, xxA, xIx, xxN, xIx, /* arabic number foll R      */ },
{ /*re*/ xxx, xxx, xxx, xxx, xxE, xxR, xxE, xIx, xIx, xxE, /* european number foll R    */ },
{ /*la*/ xxx, xxx, xxx, xxx, xxL, xxR, xxA, xIx, xxN, xIx, /* arabic number foll L      */ },
{ /*le*/ xxx, xxx, xxx, xxx, xxL, xxR, xxL, xIx, xIx, xxL, /* european number foll L    */ },

{ /*ac*/ Nxx, Nxx, Nxx, Axx, AxA, NxR, NxN, NxN, NxN, NxN, /* CS following cn           */ },
{ /*rc*/ Nxx, Nxx, Nxx, Axx, NxE, NxR, NxN, NxN, NxN, NIx, /* CS following ra           */ },
{ /*rs*/ Nxx, Nxx, Nxx, Nxx, ExE, NxR, NxN, NxN, NxN, NIx, /* CS,ES following re        */ },
{ /*lc*/ Nxx, Nxx, Nxx, Axx, NxL, NxR, NxN, NxN, NxN, NIx, /* CS following la           */ },
{ /*ls*/ Nxx, Nxx, Nxx, Nxx, LxL, NxR, NxN, NxN, NxN, NIx, /* CS,ES following le        */ },

{ /*ret*/xxx, xxx, xxx, xxx, xxE, xxR, xxE, xxN, xxN, xxE, /* ET following re           */ },
{ /*let*/xxx, xxx, xxx, xxx, xxL, xxR, xxL, xxN, xxN, xxL, /* ET following le           */ },
};

inline uint8    GetDeferredType(bidi_action a)          { return (a >> 4) & 0xF; }
inline uint8    GetResolvedType(bidi_action a)          { return a & 0xF; }
inline DirCode  EmbeddingDirection(int l)               { return l & 1 ? R : L; }
inline bool     IsDeferredState(bidi_state a)           { return 0 != ((1 << a) & (rtmask | ltmask | acmask | rcmask | rsmask | lcmask | lsmask)); }
inline bool     IsModifiedClass(DirCode a)              { return 0 != ((1 << a) & (ALmask | NSMmask | ESmask | CSmask | ETmask | ENmask)); }

void SetDeferredRunClass(Slot *s, Slot *sRun, int nval)
{
    if (!sRun || s == sRun) return;
    for (Slot *p = s->prev(); p != sRun; p = p->prev())
        p->setBidiClass(nval);
}

void resolveWeak(int baseLevel, Slot *s)
{
    int state = (baseLevel & 1) ? xr : xl;
    int cls;
    int level = baseLevel;
    Slot *sRun = NULL;
    Slot *sLast = s;

    for ( ; s; s = s->next())
    {
        sLast = s;
        cls = s->getBidiClass();
        if (cls == BN)
        {
            s->setBidiLevel(level);
            if (!s->next() && level != baseLevel)
                s->setBidiClass(EmbeddingDirection(level));
            else if (s->next() && level != s->next()->getBidiLevel() && s->next()->getBidiClass() != BN)
            {
                int newLevel = s->next()->getBidiLevel();
                if (level > newLevel)
                    newLevel = level;
                s->setBidiLevel(newLevel);
                s->setBidiClass(EmbeddingDirection(newLevel));
                level  = s->next()->getBidiLevel();
            }
            else
                continue;
        }

        bidi_action action = actionWeak[state][bidi_class_map[cls]];
        int clsRun = GetDeferredType(action);
        if (clsRun != XX)
        {
            SetDeferredRunClass(s, sRun, clsRun);
            sRun = NULL;
        }
        int clsNew = GetResolvedType(action);
        if (clsNew != XX)
            s->setBidiClass(clsNew);
        if (!sRun && (IX & action))
            sRun = s->prev();
        state = stateWeak[state][bidi_class_map[cls]];
    }

    cls = EmbeddingDirection(level);
    int clsRun = GetDeferredType(actionWeak[state][bidi_class_map[cls]]);
    if (clsRun != XX)
        SetDeferredRunClass(sLast, sRun, clsRun);
}

// Neutrals
enum neutral_action
{
        // action to resolve previous input
        nL = L,         // resolve EN to L
        En = 3 << 4,    // resolve neutrals run to embedding level direction
        Rn = R << 4,    // resolve neutrals run to strong right
        Ln = L << 4,    // resolved neutrals run to strong left
        In = (1<<8),    // increment count of deferred neutrals
        LnL = (1<<4)+L, // set run and EN to L
};

int GetDeferredNeutrals(int action, int level)
{
        action = (action >> 4) & 0xF;
        if (action == (En >> 4))
            return EmbeddingDirection(level);
        else
            return action;
}

int GetResolvedNeutrals(int action)
{
        action = action & 0xF;
        if (action == In)
            return 0;
        else
            return action;
}

// state values
enum neutral_state
{
        // new temporary class
        r,  // R and characters resolved to R
        l,  // L and characters resolved to L
        rn, // N preceded by right
        ln, // N preceded by left
        a,  // AN preceded by left (the abbrev 'la' is used up above)
        na, // N preceeded by a
} ;

const uint8 neutral_class_map[] = { 0, 1, 2, 0, 4, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0 };

const int actionNeutrals[][5] =
{
//      N,      L,      R, AN, EN, = cls
                                // state =
{       In,  0,  0,  0,  0, },  // r    right
{       In,  0,  0,  0,  L, },  // l    left

{       In, En, Rn, Rn, Rn, },  // rn   N preceded by right
{       In, Ln, En, En, LnL, }, // ln   N preceded by left

{       In,  0,  0,  0,  L, },  // a   AN preceded by left
{       In, En, Rn, Rn, En, },  // na   N  preceded by a
} ;

const int stateNeutrals[][5] =
{
//       N, L,  R,      AN, EN = cls
                                                // state =
{       rn, l,  r,      r,      r, },           // r   right
{       ln, l,  r,      a,      l, },           // l   left

{       rn, l,  r,      r,      r, },           // rn  N preceded by right
{       ln, l,  r,      a,      l, },           // ln  N preceded by left

{       na, l,  r,      a,      l, },           // a  AN preceded by left
{       na, l,  r,      a,      l, },           // na  N preceded by la
} ;

void resolveNeutrals(int baseLevel, Slot *s)
{
    int state = baseLevel ? r : l;
    int cls;
    Slot *sRun = NULL;
    Slot *sLast = s;
    int level = baseLevel;

    for ( ; s; s = s->next())
    {
        sLast = s;
        cls = s->getBidiClass();
        if (cls == BN)
        {
            if (sRun)
                sRun = sRun->prev();
            continue;
        }

        int action = actionNeutrals[state][neutral_class_map[cls]];
        int clsRun = GetDeferredNeutrals(action, level);
        if (clsRun != N)
        {
            SetDeferredRunClass(s, sRun, clsRun);
            sRun = NULL;
        }
        int clsNew = GetResolvedNeutrals(action);
        if (clsNew != N)
            s->setBidiClass(clsNew);
        if (!sRun && (action & In))
            sRun = s->prev();
        state = stateNeutrals[state][neutral_class_map[cls]];
        level = s->getBidiLevel();
    }
    cls = EmbeddingDirection(level);
    int clsRun = GetDeferredNeutrals(actionNeutrals[state][neutral_class_map[cls]], level);
    if (clsRun != N)
        SetDeferredRunClass(sLast, sRun, clsRun);
}

const int addLevel[][4] =
{
                //    L,  R,    AN, EN = cls
                                                // level =
/* even */      { 0,    1,      2,      2, },   // EVEN
/* odd  */      { 1,    0,      1,      1, },   // ODD

};

void resolveImplicit(Slot *s, Segment *seg, uint8 aMirror)
{
    bool rtl = seg->dir() & 1;
    for ( ; s; s = s->next())
    {
        int cls = s->getBidiClass();
        if (cls == BN)
            continue;
        else if (cls == AN)
            cls = AL;
        if (cls < 5 && cls > 0)
        {
            int level = s->getBidiLevel();
            level += addLevel[level & 1][cls - 1];
            s->setBidiLevel(level);
            if (aMirror)
            {
                int hasChar = seg->glyphAttr(s->gid(), aMirror + 1);
                if ( ((level & 1) && (!(seg->dir() & 4) || !hasChar)) 
                  || ((rtl ^ (level & 1)) && (seg->dir() & 4) && hasChar) )
                {
                    unsigned short g = seg->glyphAttr(s->gid(), aMirror);
                    if (g) s->setGlyph(seg, g);
                }
            }
        }
    }
}

void resolveWhitespace(int baseLevel, Segment *seg, uint8 aBidi, Slot *s)
{
    for ( ; s; s = s->prev())
    {
        int cls = seg->glyphAttr(s->gid(), aBidi);
        if (cls == WS)
            s->setBidiLevel(baseLevel);
        else
            break;
    }
}


inline
Slot * join(int level, Slot * a, Slot * b)
{
    if (!a) return b;
    if (level & 1)  { Slot * const t = a; a = b; b = t; }
    Slot * const t = b->prev();
    a->prev()->next(b); b->prev(a->prev()); // splice middle
    t->next(a); a->prev(t);                 // splice ends
    return a;
}


Slot * span(Slot * & cs, const bool rtl)
{
    Slot * r = cs, * re = cs; cs = cs->next();
    if (rtl)
    {
        Slot * t = r->next(); r->next(r->prev()); r->prev(t);
        for (int l = r->getBidiLevel(); cs && l == cs->getBidiLevel(); cs = cs->prev())
        {
            re = cs;
            t = cs->next(); cs->next(cs->prev()); cs->prev(t);
        }
        r->next(re);
        re->prev(r);
        r = re;
    }
    else
    {
        for (int l = r->getBidiLevel(); cs && l == cs->getBidiLevel(); cs = cs->next())
            re = cs;
        r->prev(re);
        re->next(r);
    }
    if (cs) cs->prev(0);
    return r;
}


Slot *resolveOrder(Slot * & cs, const bool reordered, const int level)
{
    Slot * r = 0;
    int ls;
    while (cs && level <= (ls = cs->getBidiLevel() - reordered))
    {
        r = join(level, r, level >= ls
                                ? span(cs, level & 1)
                                : resolveOrder(cs, reordered, level+1));
    }
    return r;
}

