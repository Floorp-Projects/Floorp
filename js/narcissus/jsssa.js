/* vim: set sw=4 ts=4 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Narcissus JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Brendan Eich <brendan@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Narcissus - JS implemented in JS.
 *
 * SSA builder and optimizations.
 */

(function() {

    const parser = Narcissus.parser;
    const definitions = Narcissus.definitions;
    const tokens = definitions.tokens;
    const Node = parser.Node;
    const hasOwnProperty = Object.prototype.hasOwnProperty;

    // Set constants in the local scope.
    eval(definitions.consts);

    const dash = new Node({ lineno: -1 }, INTERVENED);

    /*
     * Helper for using native JS objects as hashes.
     */
    function GenHash(seed) {
        this.seed = seed || 1;
    }

    GenHash.prototype = {
        gen: function(name) {
            // | is an illegal JS identifier character, so we don't risk the
            // generated hash being an actual identifier.
            return name + "|" + this.seed++;
        }
    }

    const genhash = new GenHash();

    /*
     * FLow analysis helper data structure.
     */

    function Upvars(defs, uses, useTuples, intervenes, escapes, flowIUVs,
                    needsEscape, isEval) {
        this.__hash__ = genhash.gen("$upvars");
        this.defs = defs || new Map;
        this.uses = uses || new Map;
        this.useTuples = useTuples || new Map;
        this.intervenes = intervenes || new Map;
        this.escapes = escapes || new Map;

        // For flowing upvars.
        this.flowIUVs = flowIUVs || new Map;

        this.needsEscape = needsEscape;
        this.isEval = isEval;
    }

    Upvars.prototype = {
        clone: function() {
            return new Upvars(this.defs.clone(),
                              this.uses.clone(),
                              this.useTuples.clone(),
                              this.intervenes.clone(),
                              this.escapes.clone(),
                              this.flowIUVs.clone(),
                              this.needsEscape,
                              this.isEval);
        },

        unionWith: function(r) {
            this.defs.unionWith(r.defs);
            this.uses.unionWith(r.uses);
            this.useTuples.unionWith(r.useTuples);
            this.intervenes.unionWith(r.intervenes);
            this.escapes.unionWith(r.escapes);
            this.flowIUVs.unionWith(r.flowIUVs);
            this.needsEscape = this.needsEscape || r.needsEscape;
            this.isEval = this.isEval || r.isEval;
        },

        // Calculates the set of intervened vars transitively. A null means
        // "everything", i.e. eval.
        transClosureI: function(visited) {
            var r = this.defs.clone();
            var members = this.intervenes.members;
            var uv;

            // We can have mutually recursive functions which cause cycles:
            //
            // function f() {
            //   var x, h;
            //   function g() { h(); }
            //   h = function() { g(); };
            //   g();
            // }
            //
            // So we need to keep track of all the upvars we've already
            // visited.
            visited.insert1(this);

            var uvs;
            for (var i in members) {
                uv = members[i];

                // If the upvar was intervened, we must give up and consider
                // all upvars to have been invalidated.
                if (uv.def === dash) {
                    return null;
                }

                uvs = uv.upvars;
                if (uvs && !visited.lookup(uvs)) {
                    var tci;
                    if (tci = uvs.transClosureI(visited/*, d+1*/)) {
                        r.unionWith(tci);
                    } else {
                        return null;
                    }
                }
            }

            return r;
        },

        // Calculates the set of escaped vars transitively. A null means
        // "everything", i.e. eval.
        transClosureE: function(visited) {
            var r = this.escapes.clone();
            var members = r.members;
            var uv;

            visited.insert1(this);

            // The transitive closure of escapes is all the union of the
            // transitive closures of the _interventions_ of those escapes
            // _and_ the transitive closures of the escapes of those escapes.
            var uvs;
            for (var i in members) {
                uv = members[i];

                // If the upvar was intervened, we must give up and consider
                // all upvars to have escaped.
                if (uv.def === dash) {
                    return null;
                }

                uvs = uv.upvars;
                if (uvs && !visited.lookup(uvs)) {
                    var tci, tce;
                    if (tci = uvs.transClosureI(visited)) {
                        r.unionWith(tci);
                    } else {
                        return null;
                    }
                    if (tce = uvs.transClosureE(visited)) {
                        r.unionWith(tce);
                    } else {
                        return null;
                    }
                }
            }

            return r;
        }
    }

    // Entry for a variable, includes flow information.
    //
    // type ::= VAR | CONST | LET
    function Current(name, type, def, upvars) {
        def = def || mkRawIdentifier({ lineno: -1 }, "undefined", null, true);

        this.__hash__ = genhash.gen(name);
        this.name = name;
        this.type = type;
        this._def = def;
        this.upvars = upvars;
        this.gotIntervened = false;
    }

    Current.prototype = {
        clone: function() {
            var c = new Current(this.name,
                                this.type,
                                this.def,
                                this.upvars.clone());
            c.gotIntervened = this.gotIntervened;
            return c;
        },

        get def() {
            return this.gotIntervened ? dash : this._def;
        },

        set def(d) {
            this._def = d;
        }
    };

    // Map that can be used as a set.
    function Map() {
        this.members = {};
        this.length = 0;
    }

    function h(k) {
        return k.__hash__ ? k.__hash__ : k;
    }

    Map.prototype = {
        clone: function() {
            var c = new Map;
            c.unionWith(this);
            return c;
        },

        lookup: function(k) {
            return this.members[h(k)];
        },

        insert: function(k, v) {
            k = h(k);
            if (!hasOwnProperty.call(this.members, k)) {
                this.members[k] = v;
                this.length++;
            }
        },

        insert1: function(k) {
            this.insert(k, k);
        },

        remove: function(k) {
            k = h(k);
            if (hasOwnProperty.call(this.members, k)) {
                delete this.members[k];
                this.length--;
            }
        },

        unionWith: function(r) {
            var members = r.members;
            var keys = Object.getOwnPropertyNames(members);
            for (var i in keys) {
                var k = keys[i];
                this.insert(k, members[k]);
            }
        },

        clear: function() {
            this.members = {};
            this.length = 0;
        }
    }

    // Bindings attached to each scope level.
    function Bindings(p, isFunction, isWith) {
        this.parent = p;
        this.isFunction = isFunction;
        this.isWith = isWith;
        this.currents = {};
        this.params = {};
        this.possibleHoists = {};
        this.dead = false;
        this.inRHS = 0;
        if (isWith) {
            this.withIntervenes = new Map;
        }
        if (isFunction) {
            // frees is a superset of upvars
            this.frees = {};
            this.upvars = new Upvars;
            this.backpatchUpvars = new Map;
            // The set of variables that have escaped permanently. Monotonic
            // per function.
            this.escaped = new Upvars;
            // Backup values for upvars that were assigned to.
            this.upvarOlds = {};
            // Nodes for recursive function calls.
            this.backpatchMus = [];
        }

        // Cache nearest function.
        var r = this;
        while (!r.isFunction) {
            r = r.parent;
        }
        this.nearestFunction = r;
    }

    Bindings.prototype = {
        escapedVars: function() {
            if (this.evalEscaped) {
                return null;
            }

            return this.escaped.transClosureI(new Map);
        },

        declareParam: function(x) {
            this.params[x] = true;
        },

        declareVar: function(x, tt, isExternal) {
            if (this.dead) {
                return;
            }

            var fb = this.nearestFunction;
            var c = this.current(x);

            if (c) {
                if (c.type == LET) {
                    throw new TypeError("Cannot redeclare a let to a var");
                }
                if (c.type == CONST && tt == CONST) {
                    throw new TypeError("redeclaration of const " + x);
                }
            } else if (!hasOwnProperty.call(fb.params, x)) {
                // Don't redeclare vars; just don't error. This is important
                // during hoisting to have hoisted functions intervene properly.
                //
                // We can also shadow upvar clones.
                //
                // If we don't know of a variable, we need to put it all the
                // way at the top (the enclosing function's context) with a value
                // of "undefined".
                var fb = this.nearestFunction;
                fb.currents[x] = c = new Current(x, tt);
                c.internal = !isExternal;

                return c.def;
            }
        },

        declareLet: function(x, hoisted, isExternal) {
            if (this.dead) {
                return;
            }

            var c = this.currents[x];
            if (c) {
                if (!c.hoisted)
                    throw new TypeError("Cannot redeclare a let");
                // Okay to redeclare a hoisted let once.
                c.hoisted = false;
                return;
            }

            c = this.currents[x] = new Current(x, LET);
            c.hoisted = hoisted;
            c.internal = !isExternal;

            return c.def;
        },

        update: function(x, def, upvars) {
            if (this.dead) {
                return;
            }

            var c = this.current(x);
            if (!c) {
                // Backup upvar assignments.
                c = this.upvar(x);
                var p = this.nearestFunction;
                if (!hasOwnProperty.call(p.upvarOlds, x)) {
                    p.upvarOlds[x] = { def: c.def,
                                       upvars: c.upvars.clone(),
                                       gotIntervened: c.gotIntervened };
                }
            }
            if (c.type == CONST) {
                return;
            }

            // New assignments clear intervention.
            c.gotIntervened = false;
            c.def = def;
            c.upvars = upvars;
        },

        hasCurrent: function(x) {
            var p = this.isFunction ? null : this.parent;
            return hasOwnProperty.call(this.currents, x) ||
                   (p && p.hasCurrent(x));
        },

        hasParam: function(x) {
            var p = this.nearestFunction;
            return hasOwnProperty.call(p.params, x);
        },

        hasUpParam: function(x) {
            var p = this.nearestFunction;
            p = p.parent;
            if (p) {
                var r = p.hasParam(x);
                return r || p.hasUpParam(x);
            } else {
                return null;
            }
        },

        upvar: function(x) {
            // Find the closest bindings above the current function toplevel.
            //
            // We don't consider upvars to be current, or SSA-able,
            // immediately at parse time inside an inner function. We'd have
            // to prove a lot to get that property, i.e. the set of all inner
            // functions whose upvar-set include the current upvar in question
            // don't escape.
            var p = this.nearestFunction;
            p = p.parent;
            if (p) {
                var c = p.current(x);
                // Don't return clones, only originals.
                return c || p.upvar(x);
            } else {
                return null;
            }
        },

        addUpvar: function(tt, uv) {
            this.nearestFunction.upvars[tt].insert1(uv);
        },

        pushUpvarUse: function(uv, n) {
            var useTuples = this.nearestFunction.upvars.useTuples;
            if (!useTuples.lookup(uv)) {
                useTuples.insert(uv, { cdef: undefined, nodes: [] });
            }
            useTuples.lookup(uv).nodes.push(n);
        },

        pushMuUse: function(n) {
            this.nearestFunction.backpatchMus.push(n);
        },

        removeMuUse: function(n) {
            var nodes = this.nearestFunction.backpatchMus;
            for (var i in nodes) {
                if (n === nodes[i]) {
                    nodes[i] = nodes[nodes.length-1];
                    nodes.pop();
                    return;
                }
            }
        },

        removeUpvarUse: function(uv, n) {
            // XXX Can we do better than linear?
            var useTuples = this.nearestFunction.upvars.useTuples;
            var nodes = useTuples.lookup(uv).nodes;
            for (var i in nodes) {
                if (n === nodes[i]) {
                    // Since ordering doesn't matter here, swap it with the
                    // last element and pop.
                    nodes[i] = nodes[nodes.length-1];
                    nodes.pop();
                    return;
                }
            }
        },

        addBackpatchUpvars: function(upvars) {
            this.nearestFunction.backpatchUpvars.insert1(upvars);
        },

        closureIsEval: function() {
            // This is to signal that if the current function becomes a
            // closure, when it is called, it invalidates everything in the
            // current scope chain like an eval.
            this.nearestFunction.isEval = true;
        },

        closureNeedsEscape: function() {
            // This is to signal that if the current function becomes a
            // closure, when it is called, it also needs to invalidate the set
            // of all escaped variables because some unknown identifier was
            // called inside the function.
            this.nearestFunction.needsEscape = true;
        },

        addToFrees: function(x) {
            this.nearestFunction.frees[x] = true;
        },

        declareFunction: function(x, f, frees, upvars) {
            var fb = this.nearestFunction;
            if (!fb.currents[x]) {
                fb.currents[x] = new Current(f.name, VAR, f, upvars);
            }
        },

        mu: function(x) {
            // Is x a recursive call to a function in the current function
            // stack?
            var r = this;
            while (r) {
                var fb = r.nearestFunction;
                if (fb.name == x) {
                    return fb;
                } else {
                    r = r.parent;
                }
            }

            return null;
        },

        current: function(x) {
            var r = undefined;
            if (hasOwnProperty.call(this.currents, x)) {
                r = this.currents[x];
            } else if (!this.isFunction && this.parent) {
                // Only look up in parent if we don't cross function
                // boundaries.
                r = this.parent.current(x);
            }

            return r;
        },

        unionUpTo: function(upTo) {
            var vars = new Map;
            var p = this;
            while (p) {
                var currents = p.currents;
                var cs = Object.getOwnPropertyNames(currents);
                var c;
                for (var i in cs) {
                    c = currents[cs[i]];
                    // Consts don't need to be intervened.
                    if (c.type != CONST) {
                        vars.insert1(c);
                    }
                }
                if (p === upTo)
                    return vars;
                else
                    p = p.parent;
            }

            // If upto were null, i.e. "everything".
            return vars;
        },

        intervene: function(vars) {
            // If we're to intervene everything, we do it recursively, even
            // across function boundaries.
            if (!vars) {
                vars = this.unionUpTo(null);
            }

            var uv, intervened = [];
            var members = vars.members;
            for (var i in members) {
                uv = members[i];
                // If we intervened an upvar, be sure to save the old value to
                // restore when the function finishes parsing.
                if (!this.hasCurrent(uv.name)) {
                    var p = this.nearestFunction;
                    var x = uv.name;

                    if (uv.upvars) {
                        // At this point any flow upvars have escaped.
                        var fiuvs = uv.upvars.flowIUVs.members;
                        for (var j in fiuvs) {
                            this.addUpvar("escapes", fiuvs[j]);
                        }
                    }

                    if (!hasOwnProperty.call(p.upvarOlds, x)) {
                        p.upvarOlds[x] = { def: uv.def,
                                           upvars: uv.upvars,
                                           gotIntervened: uv.gotIntervened };
                    }
                }
                // Save the original definition along with the pointer of the
                // upvar it belongs to.
                intervened.push({ ptr: uv,
                                  def: uv.def,
                                  upvars: uv.upvars, });
                uv.def = undefined;
                uv.upvars = undefined;
                // Intervening 2+ times in a row can cause old to be
                // improperly defined in phis.
                uv.gotIntervened = true;
            }

            return intervened;
        },

        rememberPossibleHoist: function(x) {
            //
            // Taxonomy of hoists:
            //
            // If we don't know anything about it, it's a var hoist.
            //   If we are in a non-toplevel block, it's also a let hoist.
            // Else
            //   If we are in a non-toplevel block, it's a let hoist.
            //
            // Hoists propagate backwards when blocks finish, i.e.
            //
            // { 1
            //   { 2
            //     x;
            //   }
            // }
            //
            // When we finish parsing block 2, x needs to be propagated as a let
            // hoist to block 1 as well.
            //
            this.possibleHoists[x] = true;
        },

        isPossibleHoist: function(x) {
            return hasOwnProperty.call(this.possibleHoists, x) &&
                   !hasOwnProperty.call(this.params, x);
        },

        propagatePossibleHoists: function() {
            var p = this.inFunction ? this.parent.nearestFunction
                                    : this.parent;
            if (!p)
                return;

            var hoists = Object.getOwnPropertyNames(this.possibleHoists);
            var x;
            for (var i in hoists) {
                x = hoists[i];
                if (!hasOwnProperty.call(p.currents, x)) {
                    p.rememberPossibleHoist(x);
                }
            }
        },
    };

    function SSAJoin(p, b, before) {
        this.parent = p;
        this.binds = b;
        this.hasJoinBefore = before;
        this.branch = 0;
        this.phis = {};
        this.uses = {};

        // this.binds is the _parent_ bindings for this join. Each branch will
        // have a different set of bindings!
    }

    SSAJoin.mkJoin = function(ps, parent, branches, propagate) {
        // No point in doing phi if no branches branch.
        if (branches < 1) {
            return null;
        }

        var e, e2, rhs;

        function phiAssignment(x, operands) {
            var e, lhs, rhs;
            var ft = { lineno: -1 };

            lhs = new Node(ft, IDENTIFIER);
            lhs.value = x;
            // Set local to help decomp do value numbering.
            lhs.local = operands.type;
            rhs = new Node(ft, PHI);
            rhs.__hash__ = genhash.gen("$phi");

            // Optimize away phi nodes where every branch has been intervened, we
            // don't even need to propagate.
            var allDashes = true;
            var os, o = null;
            for (var i = branches - 1; i >= 0; i--) {
                o = operands[i] ? operands[i].def : operands.old.def;
                if (o.type != INTERVENED)
                    allDashes = false;
                rhs.push(o);
            }
            if (allDashes) {
                return null;
            }
            rhs.reverse();

            e = new Node(ft, ASSIGN);
            e.push(lhs);
            e.push(rhs);

            return e;
        }

        e = new Node({ lineno: -1 }, COMMA);
        for (var x in ps) {
            if (!(e2 = phiAssignment(x, ps[x]))) {
                continue;
            }

            rhs = e2[1];

            // Optimize away phis that are only one branch, but we still need
            // to propagate them!
            if (branches == 1) {
                rhs = rhs[0];
            } else {
                // Push a phi use for each operand so the phis can be filled
                // in during exec.
                for (var i = 0, j = rhs.length; i < j; i++) {
                    if (rhs[i].type == INTERVENED) {
                        rhs.intervened = true;
                    }
                    rhs[i].pushPhiUse(rhs);
                }
                e.push(e2);
            }

            propagate(x, rhs);
        }

        return e.length > 0 ? e : null;
    }

    SSAJoin.prototype = {
        nearestTryJoin: function() {
            if (this.isTry) {
                return this;
            } else if (this.parent) {
                return this.parent.nearestTryJoin();
            } else {
                return null;
            }
        },

        unionPhisUpTo: function(joinUpTo) {
            var ps = {}, join = this, binds = join.binds;
            var bindsUpTo = joinUpTo.binds;
            while (join) {
                for (var x in join.phis) {
                    //
                    // Only insert phis for which the pointer for x in the up-to
                    // environment is the same as the pointer for it in the
                    // current environment. This is to prevent cases like the
                    // following:
                    //
                    // var x = 0;
                    // while (e1) {
                    //   x = 1;
                    //   {
                    //     let x = 0;
                    //     if (e2) {
                    //       x = 2;
                    //       break;
                    //     } else {
                    //       x = 3;
                    //       break;
                    //     }
                    //   }
                    // }
                    //
                    // Even though in the join for the if x has a phi, it
                    // shouldn't be counted for the break since it's a phi for the
                    // let x. Only the x = 1 phi at the while level should be
                    // counted.
                    //
                    if (bindsUpTo.current(x) === binds.current(x)) {
                        ps[x] = join.phis[x];
                    }
                }
                if (join === joinUpTo) {
                    break;
                }
                join = join.parent;
                binds = join.binds;
            }
            return ps;
        },

        shiftPhis: function() {
            var ps = this.phis;
            for (var x in ps) {
                ps[x].shift();
            }
            this.branch--;
        },

        killBranch: function(currentBinds, killJoin, isThrow) {
            // killJoin is null for return.
            currentBinds.dead = true;

            // If we are inside of a try block and we are not a throw, then we
            // create a new branch in the finally join node.
            var tryJoin = this.nearestTryJoin();
            if (tryJoin && tryJoin&& !isThrow) {
                var oldKillJoin = killJoin;
                killJoin = tryJoin.isFinally ? tryJoin : tryJoin.parent;
            }

            if (!killJoin) {
                this.dead = true;
                return;
            }

            var killBinds = killJoin.binds;
            // Need to manually calculate the union of all phi nodes between here
            // and killJoin.
            var ps = this.unionPhisUpTo(killJoin);

            // Propagate the phis up to the break environment.
            var c, old;
            for (var x in ps) {
                //
                // For breaks we _only_ insert a phi branch; we do _not_ remember
                // it as the current value.
                //
                // The variable also could've gotten intervened on the way:
                // var x = 0;
                // try {
                //   if (cond) {
                //     eval(foo);
                //   }
                // } catch (e) {
                // }
                //
                c = killBinds.current(x) || killBinds.upvar(x);
                old = ps[x].old;
                killJoin.insertPhi(c.type, x, c.def, c.upvars,
                                   old.def, old.upvars);
            }

            killJoin.finishBranch();
            killJoin.upkill = oldKillJoin;

            // Don't set this.dead until the end else insertPhi and finishBranch
            // won't work properly.
            this.dead = true;
        },

        finishBranch: function() {
            if (!this.dead) {
                this.branch++;
            }
        },

        commit: function() {
            // Output assignments and push phi nodes to parent?
            var r, e;
            var ps = this.phis, us = this.uses
            var parent = this.parent;
            var binds = this.binds;
            var ft = { lineno: -1 };
            var intervened = false;

            function go(x, rhs) {
                var u = us[x], uu;
                var old = ps[x].old;
                var prop = ps[x].prop;

                if (u) {
                    for (var i = 0, j = u.length; i < j; i++) {
                        //
                        // If the use pointed to the old value (before the loop)
                        // or it didn't point to anything before and is on the
                        // rhs, update it to use the phi node.
                        //
                        // NB: An identifier on the lhs would have had its forward
                        // pointer set to null instead of undefined.
                        //
                        uu = u[i];
                        // Phi nodes might have stale branches.
                        if (uu.type == PHI) {
                            for (var k = 0, l = uu.length; k < l; k++) {
                                if (uu[k] === old.def) {
                                    uu[k] = rhs;
                                    rhs.pushPhiUse(uu);
                                }
                            }
                        } else if (uu.forward === old.def) {
                            uu.forward = rhs;
                        }
                    }
                }

                var upvars = unionPhiUpvars(ps[x]);

                // If we explicitly set a value to propagate, propagate that.
                if (prop) {
                    rhs = prop.def;
                    upvars = prop.upvars;
                }

                // Propagate to parent if it's bound there.
                var type = ps[x].type;
                if (parent && (type == VAR || !binds.currents[x])) {
                    parent.insertPhi(ps[x].type, x, rhs, upvars,
                                     old.def, old.upvars);
                    //
                    // The phi node might be using stale values that need to be
                    // replaced if it's contained in a loop.
                    //
                    // function f() {
                    //     var x = 0;
                    //     while (x < 10) {
                    //         if (false) {
                    //             x = x + 1;
                    //         }
                    //         // The phi for the if will contain a
                    //         // stale forward pointer to x.
                    //         x = 1;
                    //     }
                    // }
                    //
                    parent.rememberUse(x, rhs);
                }

                binds.update(x, rhs, upvars);
            }

            e = SSAJoin.mkJoin(ps, parent, this.branch, go);
            if (!e) {
                // If every branch was killed, we need to restore, or we're going
                // to be left with the last branch's values as the currents.
                if (this.branch < 1) {
                    this.restore(binds);
                }
                return null;
            }

            r = new Node(ft, SEMICOLON);
            r.expression = e;
            return r;
        },

        // Record a use if we need to.
        rememberUse: function(x, v) {
            if (this.hasJoinBefore) {
                var uses = this.uses;
                if (!hasOwnProperty.call(uses, x)) {
                    uses[x] = [];
                }
                uses[x].push(v);
            } else {
                // Recur to the nearest enclosing parent with hasJoinBefore.
                this.parent && this.parent.rememberUse(x, v);
            }
        },

        insertDashes: function(binds, intervened) {
            var iv, uv, c, name;
            for (var x in intervened) {
                iv = intervened[x];
                uv = iv.ptr;
                name = uv.name;
                c = binds.current(name);
                // If we intervened the current copy, we need to insert a phi.
                // This will also allow us to restore.
                if (uv === c && !c.internal &&
                    (uv.type == VAR || !binds.currents[name])) {
                    this.insertPhi(c.type, name, dash, new Upvars,
                                   iv.def, iv.upvars);
                }
            }
        },

        intervene: function(binds, vars) {
            var intervened = this.intervened = binds.intervene(vars);
            this.insertDashes(binds, intervened);
            return intervened;
        },

        // Restore the backups.
        restore: function(binds) {
            binds.dead = false;
            this.dead = false;

            var intervened = this.intervened;
            if (intervened) {
                var iv;
                for (var x in intervened) {
                    iv = intervened[x];
                    iv.ptr.def = iv.def;
                    iv.ptr.upvars = iv.upvars;
                }
            }

            var ps = this.phis;
            var old;
            for (var x in ps) {
                old = ps[x].old;
                binds.update(x, old.def, old.upvars);
            }
        },

        // old need only be passed in if the insertPhi is possibly a first phi for
        // a join.
        insertPhi: function(type, x, def, upvars,
                            oldDef, oldUpvars,
                            propDef, propUpvars) {
            var n, ps = this.phis;
            var psx;

            if (!ps || this.dead) {
                return;
            }
            if (!ps[x]) {
                psx = ps[x] = [];
                psx.use = [];
            } else {
                psx = ps[x];
            }

            psx[this.branch] = { def: def, upvars: upvars };
            // Assignments in loop conditions.
            if (propDef) {
                psx.prop = { def: propDef, upvars: propUpvars };
            }
            if (!psx.old) {
                psx.old = { def: oldDef, upvars: oldUpvars };
            }
            if (!psx.type) {
                psx.type = type;
            }
        }
    };

    /*
     * SSA builder.
     */

    function extendBuilder(child, super) {
        var childProto = child.prototype,
            superProto = super.prototype;
        for (var ns in super.prototype) {
            var childNS = childProto[ns];
            var superNS = superProto[ns];
            var childNSType = typeof childNS;
            if (childNSType === "undefined") {
                childProto[ns] = superNS;
            } else if (childNSType === "object") {
                for (var m in superNS) {
                    let childMethod = childNS[m];
                    let superMethod = superNS[m];
                    if (typeof childMethod === "undefined") {
                        childNS[m] = superMethod;
                    } else {
                        childNS[m] = function() {
                            if (this.binds)
                                return childMethod.apply(this, arguments);
                            else
                                return superMethod.apply(this, arguments);
                        };
                    }
                }
            }
        }
    }

    function SSABuilder() {
        parser.bindSubBuilders(this, SSABuilder.prototype);

        this.binds = null;
        this.join = null;
        this.hoists = {};
        this.inJoinPostDom = [false];
        this.finallyKills = [];
        this.joinStack = [];

        this.destructuringTmpFresh = 0;
        this.postfixTmpFresh = 0;
    }

    //
    // These methods are guarded to only execute if we're inside a function.
    //

    SSABuilder.prototype = {
        IF: {
            setCondition: function(n, e) {
                n.condition = e;
                // We parsed the condition in the parent context because the
                // join for if comes after.
                this.join = new SSAJoin(this.join, this.binds, false);
            },

            setThenPart: function(n, s) {
                var join = this.join;
                n.thenPart = s;
                join.finishBranch();
                join.restore(this.binds);
            },

            finish: function(n) {
                var join = this.join;
                this.join = join.parent;
                join.finishBranch();
                n.ssaJoin = join.commit();
            }
        },

        SWITCH: {
            setDiscriminant: function(n, e) {
                var join = this.join = new SSAJoin(this.join, this.binds, false);
                n.discriminant = e;
                n.breakJoin = join;
                n.continueJoin = join;
                this.fallthrough = false;
            },

            addCase: function(n, n2) {
                var join = this.join;
                if (join.dead) {
                    //
                    // The previous branch is dead (i.e. we had a break on the
                    // current case so it was killed and added a new branch to
                    // the switch)
                    //
                    join.restore(this.binds);
                    this.fallthrough = false;
                } else {
                    // Is fallthrough, mark next branch as needing a join.
                    this.fallthrough = true;
                }
                n.cases.push(n2);
            },

            finish: function(n) {
                var join = this.join;
                this.join = join.parent;
                if (this.fallthrough)
                    join.finishBranch();
                n.ssaJoin = join.commit();
            }
        },

        CASE: {
            initializeStatements: function(n, t) {
                n.statements = new Node(t, BLOCK);
                // Do we need to generate phi nodes due to fallthrough?
                if (this.fallthrough) {
                    n.ssaJoin = fallthroughJoin(n, this.join, this.binds);
                }
            }
        },

        DEFAULT: {
            initializeStatements: function(n, t) {
                n.statements = new Node(t, BLOCK);
                // Do we need to generate phi nodes due to fallthrough?
                if (this.fallthrough) {
                    n.ssaJoin = fallthroughJoin(n, this.join, this.binds);
                }
            }
        },

        FOR: {
            build: function(t) {
                var n = new Node(t, FOR);
                //
                // scB serves as the parent context so we can remember the old
                // values of variables if we're parsing a `for in' statement.
                // for in's are weird because we don't parse things in
                // dominator order.
                //
                // In `for (e1 in e2)', e1 doesn't dominate e2. i.e. in
                //
                // for (var i in is.concat([i]))
                //
                // the i on the right hand of the `in' gets evaluated once to
                // its value before the for statement.
                //
                var breakJoin = new SSAJoin(this.join, this.binds, false);
                var continueJoin = new SSAJoin(breakJoin, this.binds, true);
                n.isLoop = true;
                n.breakJoin = breakJoin;
                n.continueJoin = continueJoin;
                continueJoin.finishBranch();
                breakJoin.finishBranch();
                this.join = breakJoin;

                return n;
            },

            setObject: function(n, e) {
                var t = n.tokenizer;
                var itrhs = mkCall("iterator", e);
                // This may be changed later.
                n.setup = mkDecl(this, "LET", t, "$it", itrhs, false);
                n.condition = mkCall("hasNext", mkIdentifier(this, t, "$it"));
            },

            setSetup: function(n, e) {
                n.setup = e || null;
                this.join = n.continueJoin;
                this.inJoinPostDom.push(true);
            },

            setCondition: function(n, e) {
                n.condition = e;
                this.inJoinPostDom.pop();
            },

            setIterator: function(n, e, e2, s) {
                var setup = n.setup;

                //
                // For `for in' loops, we also parse the right of the `in'
                // expression in the parent (break) bindings.
                //
                // We do the following online transform of for in loops:
                //
                // for (e1 in e2) {
                //    body;
                // }
                //
                // If e1 is "var x":
                //
                // for (let it = e2; hasNext(it); ) {
                //     var x = next(it);
                //     body;
                // }
                //
                // If e1 is "let x":
                //
                // for (let it = e2, x; hasNext(it); ) {
                //     x = next(it);
                //     body;
                // }
                //
                // Else,
                //
                // for (var it = e2; hasNext(it); ) {
                //     e1 = next(it);
                //     body;
                // }
                //

                var dexp = e.exp;
                var decl = e.decl;
                var t = dexp ? dexp.tokenizer : e.tokenizer;
                var rhs = mkCall("next", mkIdentifier(this, t, "$it"));
                if (e2) {
                    if (dexp) {
                        if (e2.type == LET) {
                            // We need to manually increment localUses and
                            // uses due to this weird desugaring to assignment
                            // but leaving the let decl in for head that we
                            // do.
                            var bComma = this.COMMA;

                            e2.push(n.setup[0]);
                            n.setup = e2;

                            var comma = bComma.build(t);
                            desugarDestructuringAssign(this, comma, dexp, rhs);
                            bComma.finish(comma);
                            n.update = comma;
                        } else /* if (e2.type == VAR) */ {
                            var bVar = this.VAR;
                            var bDecl = this.DECL;
                            bDecl.setInitializer(decl, rhs);
                            bDecl.finish(decl);
                            bVar.finish(e2);
                            n.update = e2;
                        }
                    } else if (e2.type == VAR) {
                        var bDecl = this.DECL;
                        bDecl.setInitializer(e, rhs);
                        n.update = e2;
                    } else if (e2.type == LET) {
                        n.setup.unshift(e);
                        n.update = mkAssign(this, t, e, rhs);
                    }
                } else {
                    n.update = mkAssign(this, t, e, rhs);
                }
                this.join = n.continueJoin;
            },

            setBody: function(n, s) {
                n.body = s;
                this.join.finishBranch();
            },

            finish: function(n) {
                var continueJoin = this.join, breakJoin = continueJoin.parent;
                this.join = breakJoin.parent;
                // Add update to the top if we were a for-in
                if (n.type == FOR_IN) {
                    n.body.unshift(n.update);
                    n.update = null;
                    n.type = FOR;
                }
                n.ssaJoin = continueJoin.commit();
                if (n.ssaJoin) {
                    //
                    // Make a branch for the committed phis from scC and shift
                    // the first branch out to avoid duplicate branches.
                    //
                    breakJoin.shiftPhis();
                    breakJoin.finishBranch();
                }
                n.ssaBreakJoin = breakJoin.commit();
            }
        },

        WHILE: {
            build: function(t) {
                var n = new Node(t, WHILE);
                var breakJoin = new SSAJoin(this.join, this.binds, false);
                var continueJoin = new SSAJoin(breakJoin, this.binds, true);
                n.isLoop = true;
                n.breakJoin = breakJoin;
                n.continueJoin = continueJoin;
                // Start off in the second branch since the branch entering
                // into the loop is the first branch.
                continueJoin.finishBranch();
                breakJoin.finishBranch();
                this.join = continueJoin;
                // Any assignments encounted in the while condition causes the
                // second branch to be overriding for continueJoin.
                this.inJoinPostDom.push(true);
                return n;
            },

            setCondition: function(n, e) {
                n.condition = e;
                this.inJoinPostDom.pop();
            },

            setBody: function(n, s) {
                n.body = s;
                this.join.finishBranch();
            },

            finish: function(n) {
                var continueJoin = this.join, breakJoin = continueJoin.parent;
                this.join = breakJoin.parent;
                n.ssaJoin = continueJoin.commit();
                if (n.ssaJoin) {
                    // Make a branch for the committed phis from scC and shift
                    // the first branch out to avoid duplicate branches.
                    breakJoin.shiftPhis();
                    breakJoin.finishBranch();
                }
                n.ssaBreakJoin = breakJoin.commit();
            }
        },

        DO: {
            build: function(t) {
                var n = new Node(t, DO);
                var breakJoin = new SSAJoin(this.join, this.binds, true);
                var continueJoin = new SSAJoin(breakJoin, this.binds, true);
                n.isLoop = true;
                n.breakJoin = breakJoin;
                n.continueJoin = continueJoin;
                // Start off in the second branch since the branch entering
                // into the loop is the first branch.
                continueJoin.finishBranch();
                breakJoin.finishBranch();
                this.join = continueJoin;
                // The body and the condition both post-dominate the join.
                this.inJoinPostDom.push(true);
                return n;
            },

            setBody: function(n, s) {
                n.body = s;
                this.join.finishBranch();
            },

            setCondition: function(n, e) {
                n.condition = e;
                this.inJoinPostDom.pop();
            },

            finish: function(n) {
                var continueJoin = this.join, breakJoin = continueJoin.parent;
                this.join = breakJoin.parent;
                n.ssaJoin = continueJoin.commit();
                if (n.ssaJoin) {
                    //
                    // Make a branch for the committed phis from scC and shift
                    // the first branch out to avoid duplicate branches.
                    //
                    breakJoin.shiftPhis();
                    breakJoin.finishBranch();
                }
                //
                // Commit the break phi nodes with the line of the loop
                // condition.
                //
                // Control leaves via the condition and not the join node, so
                // we should make the second operand of all phis the current
                // values.
                //
                n.ssaBreakJoin = breakJoin.commit();
            }
        },

        BREAK: {
            finish: function(n) {
                //
                // Have to kill the current branch.
                //
                // A new branch is added to the join of the break context,
                // which is emitted _after_ the break environment.
                //
                this.join.killBranch(this.binds, n.target.breakJoin);
            }
        },

        CONTINUE: {
            finish: function(n) {
                //
                // Have to kill the current branch.
                //
                // A new branch is added to the join of the continue context,
                // which is usually the normal context of the node.
                //
                this.join.killBranch(this.binds, n.target.continueJoin);
            }
        },

        TRY: {
            build: function(t) {
                var n = new Node(t, TRY);
                var finallyJoin = new SSAJoin(this.join, this.binds, false);
                var tryJoin = new SSAJoin(finallyJoin, this.binds, false);
                tryJoin.isTry = true;
                n.catchClauses = [];
                this.join = tryJoin;
                return n;
            },

            setTryBlock: function(n, s) {
                var tryJoin = this.join, finallyJoin = tryJoin.parent;
                var binds = this.binds;
                this.join = finallyJoin;
                // Set isTry here since we can throw from inside the try block
                // from function calls as well.
                finallyJoin.isTry = finallyJoin.isFinally = true;
                n.tryBlock = s;
                // Add the current value at the end of the try block to the
                // first branch of the finally block's phi nodes if the branch
                // is still live.
                if (!tryJoin.dead) {
                    var c, old;
                    for (var x in tryJoin.phis) {
                        c = binds.current(x) || binds.upvar(x);
                        old = tryJoin.phis[x].old;
                        finallyJoin.insertPhi(c.type, x, c.def, c.upvars,
                                              old.def, old.upvars);
                    }
                    finallyJoin.finishBranch();
                }
                this.tryPhis = tryJoin.commit();
                // Manually set old values because we want branch restoration
                // across branches to actually restore the phi nodes we just
                // committed.
                for (var x in tryJoin.phis) {
                    var c = binds.current(x) || binds.upvar(x);
                    finallyJoin.phis[x].old = { def: c.def, upvars: c.upvars };
                }
            },

            finishCatches: function(n) {
                var finallyJoin = this.join;
                this.join = finallyJoin.parent;
                // Speculate that we have a finally.
                n.ssaFinallyJoin = finallyJoin.commit();
                this.finallyKills.push(finallyJoin.upkill);
            },

            finish: function(n) {
                // Crazy upkill magical logic.
                var upkill = this.finallyKills.pop();
                if (upkill && !this.join.dead) {
                    var killBinds = upkill.binds;
                    var ps = this.join.unionPhisUpTo(upkill);
                    for (var x in ps) {
                        c = killBinds.current(x), old = ps[x].old;
                        upkill.insertPhi(c.type, x, c.def, c.upvars,
                                         old.def, old.upvars);
                    }
                    upkill.finishBranch();
                    this.join.dead = true;
                }

                if (!n.finallyBlock) {
                    // Move the join if we don't have a finally block since
                    // the finally block post-dominates.
                    n.ssaJoin = n.ssaFinallyJoin;
                    n.ssaFinallyJoin = undefined;
                }
            }
        },

        CATCH: {
            build: function(t) {
                // The var name of a catch is actually a let so we need to
                // make a new bindings now.
                this.binds = new Bindings(this.binds, false, false);
                this.noBindingsOnNextBlock = true;
                var n = new Node(t, CATCH);
                n.guard = null;
                return n;
            },

            setVarName: function(n, v) {
                var binds = this.binds;

                if (v.type == ARRAY_INIT || v.type == OBJECT_INIT) {
                    // Desugar destructuring catch var name
                    var t = n.tokenizer;
                    var bLet = this.LET;
                    var bDecl = this.DECL;

                    var name = this.genDestructuringSym();
                    var lets = bLet.build(t);

                    var decl = bDecl.build(t);
                    bDecl.setName(decl, v);
                    bLet.addDestructuringDecl(lets, decl);
                    var rhs = mkRawIdentifier(t, name, null, false);
                    decl.initializer = rhs;
                    bDecl.finish(decl);
                    bLet.finish(lets);

                    v = name;
                    n.destructuredLets = lets;
                }

                binds.declareLet(v);
                // The initial value can't be undefined because it's bound
                // depending on the throw. If it gets reassigned we can
                // forward it though.
                binds.update(v, null);
                binds.current(v).catchLet = true;

                n.varName = v;
            },

            finish: function(n) {
                var p;
                var join = this.join;

                if (n.destructuredLets) {
                    n.block.unshift(n.destructuredLets);
                }

                n.ssaJoin = this.tryPhis;
                join.restore(this.binds);
                // Finish branch only if we possibly had a throw
                if (join.maybeThrows) {
                    join.finishBranch();
                }
            }
        },

        THROW: {
            finish: function(n) {
                // Have to kill the current branch. Throw adds a new branch
                // only when the current join chain has a try somewhere
                // upstream.
                var join = this.join, tryJoin = join && join.nearestTryJoin();
                join && join.killBranch(this.binds, tryJoin, true);
                if (tryJoin && tryJoin.parent) {
                    tryJoin.parent.maybeThrows = true;
                }
            }
        },

        RETURN: {
            finish: function(n) {
                // Have to kill the current branch. Unlike break and continue,
                // return does not add any new branches.
                var join = this.join;
                join && join.killBranch(this.binds, null);
            }
        },

        YIELD: {
            build: function(t) {
                this.binds.inRHS++;
                return new Node(t, YIELD);
            },

            setValue: function(n, e) {
                // Yields can cause escapes.
                var join = this.join;
                var binds = this.binds;

                escapeVars(join, binds, e.upvars || new Upvars);
                n.value = e;
                --binds.inRHS;
            },

            finish: function(n) {
                // Yields don't kill the current branch, but it does yield
                // control. So we need to intervene against all escaped
                // variables thus far.
                var join = this.join;
                var binds = this.binds;
                var fb = binds.nearestFunction;
                var escaped = fb.escapedVars();

                binds.closureNeedsEscape();

                if (join) {
                    join.intervene(binds, escaped);
                } else {
                    binds.intervene(escaped);
                }
            }
        },

        FUNCTION: {
            setName: function(n, v) {
                n.name = v;
                // Need to do recursive function names. This can be shadowed
                // by params, vars, and inner functions.
                //
                // You can't assign to it, assigning to the function name
                // assigns to either the thing that shadowed it or to a look
                // up the scope chain.
                this.binds.name = v;
            },

            addParam: function(n, v) {
                n.params.push(v);
                this.binds.declareParam(v);
            },

            hoistVars: function(id, toplevel) {
                var binds = this.binds;
                var vds = this.hoists[id];

                if (toplevel) {
                    // Rip out the existing bindings and put in a new one.
                    binds = this.binds = new Bindings(binds.parent, true, false);
                    binds.noPop = true;
                    this.noBindingsOnNextBlock = true;
                }
                if (!vds)
                    return;

                var name, init;

                var vd;
                for (var i = 0, j = vds.length; i < j; i++) {
                    vd = vds[i];
                    name = vd.name;
                    if (vd.type == FUNCTION) {
                        // All function hoists are guaranteed to come after
                        // var hoists (or at least the code in jsparse.js
                        // does).
                        //
                        // We need to check if any of the names in frees have
                        // been bound, and if so, add it to upvars.
                        var frees = vd.frees;
                        var upvars = vd.upvars = new Upvars;
                        var c;

                        for (var x in frees) {
                            c = binds.current(x) || binds.upvar(x);
                            if (c) {
                                upvars.defs.insert1(c);
                                delete frees[x];
                            }
                        }

                        binds.declareFunction(name, vd, frees, upvars);
                    } else {
                        binds.declareVar(name, vd.readOnly ? CONST : VAR);
                    }
                }

                this.hoists[id] = undefined;
            },

            finish: function(n, x) {
                var binds = this.binds;
                this.binds.propagatePossibleHoists();
                var p = this.binds = binds.parent;

                n.frees = binds.frees;
                n.upvars = binds.upvars;
                n.upvars.needsEscape = binds.needsEscape;
                n.upvars.isEval = binds.isEval;

                this.join = this.joinStack.pop();

                if (p) {
                    var name = n.name;
                    if (name) {
                        var c = p.current(name);
                        var ff = n.functionForm;
                        var pfb = p.nearestFunction;

                        if (ff == parser.DECLARED_FORM) {
                            if (pfb.isPossibleHoist(name)) {
                                x.needsHoisting = true;
                            }

                            // DECLARED_FORM functions hoist _above_ vars, so
                            // if there's a existent var it won't shadow its
                            // binding.
                            if (!c) {
                                pfb.declareFunction(name, n, n.frees, n.upvars);
                            }
                        } else if (n.functionForm == parser.STATEMENT_FORM) {
                            // STATEMENT_FORM functions (i.e. in a block) are
                            // kind of like assignment because of "dynamic
                            // scope".
                            pfb.declareVar(name, VAR);
                            mkAssignSimple(this, n.tokenizer, name, n, true);
                        }
                    }

                    var uvs = Object.getOwnPropertyNames(binds.upvarOlds);
                    var old, c, x;
                    var upvarOlds = binds.upvarOlds;
                    var p2;
                    for (var i in uvs) {
                        x = uvs[i];
                        old = upvarOlds[x];

                        // Update in the bindings that has x.
                        //
                        // Because of the way intervention propagation works,
                        // however, the upvar might already be gone, so if we
                        // don't find it just ignore it.
                        p2 = p;
                        while (p2 && !p2.hasCurrent(x)) {
                            p2 = p2.parent;
                        }

                        if (p2) {
                            p2.update(x, old.def, old.upvars);
                            c = p2.current(x);
                            c.gotIntervened = old.gotIntervened;
                        }
                    }
                }

                // Do backpatching for recursive calls
                var mus = binds.backpatchMus;
                for (var i in mus) {
                    mus[i].setForward(n);
                }

                // Backpatch upvars for the ones that we proved to not escape
                // and were only written once.
                var backpatches = binds.backpatchUpvars.members;
                for (var i in backpatches) {
                    var bp = backpatches[i];
                    var uvuses = bp.uses.members;
                    for (var j in uvuses) {
                        var use = uvuses[j];
                        var useTuple = bp.useTuples.lookup(use);
                        var cdef = useTuple.cdef;

                        if (cdef === undefined)
                            continue;

                        var unodes = useTuple.nodes;
                        for (var k in unodes) {
                            // All of these are upvar forwards.
                            unodes[k].setForward(cdef, true);
                        }
                    }
                }
            }
        },

        VAR: {
            addDestructuringDecl: mkAddDestructuringDecl(VAR, false),

            addDecl: function(n, n2, x) {
                var name = n2.name;
                var binds = this.binds;

                n2.initializer = binds.declareVar(name, VAR, !n2.internal);

                if (binds.nearestFunction.isPossibleHoist(name)) {
                    x.needsHoisting = true;
                }

                n.push(n2);
                x && x.varDecls.push(mkHoistDecl(this, n2));
            }
        },

        CONST: {
            addDestructuringDecl: mkAddDestructuringDecl(CONST, true),

            addDecl: function(n, n2, x) {
                var name = n2.name;
                var binds = this.binds;

                n2.initializer = binds.declareVar(name, CONST, !n2.internal);

                if (binds.nearestFunction.isPossibleHoist(name)) {
                    x.needsHoisting = true;
                }

                n.push(n2);
                x && x.varDecls.push(mkHoistDecl(this, n2));
            }
        },

        LET: {
            addDestructuringDecl: mkAddDestructuringDecl(LET, false),

            addDecl: function(n, n2, s) {
                var name = n2.name;
                var binds = this.binds;

                n2.initializer = binds.declareLet(name, false, !n2.internal);

                if (binds.isPossibleHoist(name)) {
                    s.needsHoisting = true;
                }

                n.push(n2);
                s && s.varDecls.push(mkHoistDecl(this, n2));
            }
        },

        DECL: {
            build: function(t) {
                this.binds.inRHS++;
                return new Node(t, IDENTIFIER);
            },

            setInitializer: function(n, e) {
                if (!n.destructuredDecls) {
                    var name = n.name;
                    var c = this.binds.current(name);

                    // Treat the init as a normal assignment.
                    //
                    // c could be null because we might be trying to redeclare
                    // a param as a variable.
                    if (c) {
                        if (c.type == CONST) {
                            // Also allow initializers to update consts since
                            // they can't be redeclared anyways.
                            c.type = VAR;
                            mkAssignSimple(this, e.tokenizer,
                                           n.name, e, !n.internal);
                            c.type = CONST;
                        } else {
                            mkAssignSimple(this, e.tokenizer,
                                           n.name, e, !n.internal);
                        }
                    }

                    n.initializer = e;
                    return;
                }
                desugarDestructuringInit(this, n, e);
            },

            finish: function(n) {
                --this.binds.inRHS;
            }
        },

        LET_BLOCK: {
            build: function(t) {
                this.binds = new Bindings(this.binds, false, false);
                this.binds.noPop = true;
                this.noBindingsOnNextBlock = true;
                var n = new Node(t, LET_BLOCK);
                n.varDecls = [];
                return n;
            },

            finish: function(n) {
                this.binds.propagatePossibleHoists();
                this.binds = this.binds.parent;
            }
        },

        BLOCK: {
            build: function(t, id) {
                //
                // It's not worth it to create bindings lazily due to
                // hoisting. That is, any identifier in an inner block that
                // isn't let-bound at the same level (even if it's bound in
                // the parent scope!) is a possible let hoist.
                //
                // To keep this information then we need a bindings-like thing
                // for each block anyways, so we might as well just eagerly
                // create bindings per block.
                //
                var n = new Node(t, BLOCK);
                n.varDecls = [];
                n.id = id;
                if (this.noBindingsOnNextBlock) {
                    this.binds.block = n;
                    this.binds.id = id;
                    this.noBindingsOnNextBlock = false;
                    return n;
                }
                this.binds = new Bindings(this.binds, false, false);
                this.binds.block = n;
                this.binds.id = id;
                return n;
            },

            hoistLets: function(n) {
                var lds = this.hoists[n.id];
                if (!lds)
                    return;

                var binds = this.binds;
                var name, init;

                binds = this.binds = new Bindings(binds, false, false);
                n.seenLet = true;

                for (var i = 0, j = lds.length; i < j; i++) {
                    name = lds[i].name;
                    binds.declareLet(name, true);
                }

                this.hoists[n.id] = undefined;
            },

            finish: function(n) {
                if (this.binds.noPop)
                    return;

                this.binds.propagatePossibleHoists();
                this.binds = this.binds.parent;
            }
        },

        ASSIGN: {
            addOperand: function(n, n2) {
                if (n.length == 0) {
                    this.binds.inRHS++;
                }

                n.push(n2);
            },

            finish: function(n) {
                if (n.length == 0) {
                    return;
                }

                var join = this.join;
                var binds = this.binds;
                var fb = binds.nearestFunction;
                var lhs = n[0];
                var init = n[1];
                var upvars = init.upvars || new Upvars;

                if (--binds.inRHS > 0) {
                    n.upvars = init.upvars;
                }

                // Desugar destructuring.
                if (lhs.type == ARRAY_INIT || lhs.type == OBJECT_INIT) {
                    var t = n.tokenizer;
                    // Rebuild as COMMA.
                    n.type = COMMA;
                    n.length = 0;
                    desugarDestructuringAssign(this, n, lhs, init);
                    return;
                }

                if (lhs.type != IDENTIFIER) {
                    escapeVars(join, binds, upvars);
                    return;
                }

                // Only push phi nodes for identifiers for now, array/object
                // SSA is really hard.
                var name = lhs.value;
                var c = binds.current(name);
                var uv = binds.upvar(name);
                var hp = binds.hasParam(name);
                var hup = binds.hasUpParam(name);
                var mb = fb.mu(name);

                // Don't count lefthand side identifiers as a use.
                if (mb && (!c && !uv && !hup && !hp)) {
                    binds.removeMuUse(lhs);
                }

                // Don't trust vars inside of withs.
                if (c && binds.isWith && c.type == VAR) {
                    binds.withIntervenes.insert1(c);
                    c = null;
                    uv = null;
                }

                if (uv && !c) {
                    // Record name in the closest function binding's
                    // upvar-set.
                    binds.addToFrees(name);
                    binds.addUpvar("defs", uv);

                    // Since we're not doing flow analysis here, we assume
                    // conservatively that all upvars have escaped in the
                    // current function.
                    var suv = new Upvars;
                    suv.defs.insert1(uv);
                    escapeVars(join, binds, suv);

                    c = uv;
                }

                if (c) {
                    if (n.assignOp) {
                        // Transform op= into a normal assignment only if the
                        // lhs is an identifier we _know_ to be from a var.
                        var nt = n.tokenizer;
                        var lhs = n[0];
                        var n2 = mkRawIdentifier(nt, name, null, true);
                        this.PRIMARY.finish(n2);
                        var o = n.assignOp;
                        n.assignOp = undefined;
                        n.length = 0;
                        n.push(lhs);
                        n.push(new Node(nt, o, n2, init));
                        n2.setForward(c.def);
                        return this.ASSIGN.finish(n);
                    }

                    // Clear the forward pointer and upvars on lefthand side.
                    if (n[0].forward) {
                        n[0].forward = null;
                        n[0].upvars = null;
                    }
                    // Set local to help decomp to do value numbering.
                    n[0].local = c.type;

                    // Get the rightmost expression in case of compound
                    // assignment.
                    while (init.type == ASSIGN)
                        init = init[1];

                    if (join) {
                        // If the name is not a local let, we need a phi.
                        if (c.type == VAR) {
                            var propInit = null;
                            var propUpvars = null;
                            if (this.inJoinPostDom.top()) {
                                propInit = init;
                                propUpvars = upvars.clone();
                            }
                            join.insertPhi(VAR, name, init,
                                           upvars.clone(),
                                           c.def, c.upvars,
                                           propInit, propUpvars);
                        } else if (!binds.currents[name]) {
                            join.insertPhi(LET, name, init,
                                           upvars.clone(),
                                           c.def, c.upvars);
                        }
                    }

                    // Flow the upvars of escaped functions to the names. If
                    // the init is a lambda, it'll have upvars set on itself.
                    binds.update(name, init, upvars.clone());
                } else {
                    binds.addToFrees(name);

                    // At this point all mayEscapes in fact flowed into
                    // something we don't know about (a property, global,
                    // etc), which means they have all actually escaped.
                    escapeVars(join, binds, upvars);
                }
            }
        },

        HOOK: {
            /*
             * Basically the same thing as if, except an expression.
             */

            build: function(t) {
                var n = new Node(t, HOOK);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            setCondition: function(n, e) {
                n[0] = e;
                n.rhsUnionUpvars(e);
                this.join = new SSAJoin(this.join, this.binds, false);
            },

            setThenPart: function(n, n2) {
                var join = this.join;
                n[1] = n2;
                n.rhsUnionUpvars(n2);
                join.finishBranch();
                join.restore(this.binds);
            },

            setElsePart: function(n, n2) {
                n[2] = n2;
                n.rhsUnionUpvars(n2);
            },

            finish: function(n) {
                var join = this.join;
                this.join = join.parent;
                join.finishBranch();
                n.ssaJoin = join.commit();
            }
        },

        OR: {
            build: function(t) {
                var n = new Node(t, OR);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            addOperand: function(n, n2) {
                if (n.length == 0) {
                    // Short circuiting means the right hand expression needs
                    // to be parsed in a new context.
                    var join = this.join = new SSAJoin(this.join, this.binds, false);
                    // Start in the second branch since the first operand of a
                    // conditional is executed no matter what.
                    join.finishBranch();
                }
                n.rhsUnionUpvars(n2);
                n.push(n2);
            },

            finish: function(n) {
                var join = this.join;
                this.join = join.parent;
                join.finishBranch();
                n.ssaJoin = join.commit();
            }
        },

        AND: {
            build: function(t) {
                var n = new Node(t, AND);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            addOperand: function(n, n2) {
                if (n.length == 0) {
                    // Short circuiting means the right hand expression needs
                    // to be parsed in a new context.
                    var join = this.join = new SSAJoin(this.join, this.binds, false);
                    // Start in the second branch since the first operand of a
                    // conditional is executed no matter what.
                    join.finishBranch();
                }
                n.push(n2);
            },

            finish: function(n) {
                var join = this.join;
                this.join = join.parent;
                join.finishBranch();
                n.ssaJoin = join.commit();
            }
        },

        UNARY: {
            finish: function(n) {
                var op = n.type;
                if (n.type != INCREMENT && n.type != DECREMENT)
                    return;

                var join = this.join;
                var binds = this.binds;
                if (!(n[0].type == IDENTIFIER && binds.hasCurrent(n[0].value)))
                    return;

                //
                // Transform
                //  ++x into (x = x + 1)
                //  --x into (x = x - 1)
                //  x++ into (t = x, x = x + 1, t) for t fresh
                //  x-- into (t = x, x = x - 1, t) for t fresh
                //
                // _only_ if x is an identifier that we know to be from a var
                // or a let.
                //
                // In this case x cannot be a setter or an arbitrary side
                // effect, so we do not duplicate side effects in an unsafe
                // fashion.
                //
                var name = n[0].value;
                var c = binds.current(name);
                // Don't transform vars inside of withs
                if (binds.isWith && c.type == VAR)
                    return;

                var t = n.tokenizer;
                var ptmp = this.genPostfixSym();

                if (n.postfix) {
                    binds.block.push(mkDecl(this, "VAR", t, ptmp));
                }

                var n2 = mkRawIdentifier(t, name, null, true);
                if (c) {
                    n2.setForward(c.def);
                }

                if (join) {
                    join.rememberUse(name, n2);
                }

                if (n.postfix) {
                    n.parenthesized = true;
                    n.type = COMMA;
                    n.length = 0;
                    n.push(mkAssignSimple(this, t, ptmp,
                                          mkIdentifier(this, t, name)));
                }

                var aop = op == INCREMENT ? PLUS : MINUS;
                var assign = mkAssignSimple(this, t, name,
                                            new Node(t, aop, n2,
                                                     mkNumber(t, 1, true)));

                if (n.postfix) {
                    n.push(assign);
                    n.push(mkIdentifier(this, t, ptmp));
                } else {
                    n.type = ASSIGN;
                    n.length = 0;
                    n.push(assign[0]);
                    n.push(assign[1]);
                }
            }
        },

        MEMBER: {
            build: function(t, tt) {
                this.binds.inRHS++;
                return new Node(t, tt);
            },

            finish: function(n) {
                //
                // By fortune of the expression grammar the left side of a
                // call is never going to call MEMBER.build, and thus not
                // considered a mayEscape.
                //
                // So, anything reported to mayEscape here is going to be an
                // argument to new, called with new, or an argument to a
                // function.
                //
                var join = this.join;
                var binds = this.binds;
                var fb = binds.nearestFunction;

                if (--binds.inRHS > 0) {
                    if (unionOnRight) {
                        n.upvars = n[1].upvars;
                    } else {
                        n.upvars = n[0].upvars;
                    }
                }

                if (n.type != CALL)
                    return;

                function processUpvars(upvars) {
                    var upvarInts = upvars.transClosureI(new Map);
                    var fiuvs = upvars.flowIUVs.members;
                    for (var i in fiuvs) {
                        binds.addUpvar("intervenes", fiuvs[i]);
                    }

                    // Inner function calls can cause things to escape.
                    //
                    // We might also need the existing escaped set to be
                    // invalidated.
                    //
                    // But if it's an eval, we need to escape everything
                    // anyways, so don't do extra work.
                    if (upvars.isEval) {
                        escapeEval(join, binds);
                    } else {
                        var escs;
                        if (escs = upvars.transClosureE(new Map)) {
                            if (escs.length > 0 || upvars.needsEscape) {
                                escapeVars0(join, binds, escs, new Map);
                            }
                        }
                    }

                    var escaped = fb.escapedVars();
                    if (escaped && upvarInts && upvars.needsEscape) {
                        upvarInts.unionWith(escaped);
                    }

                    // Inner functions that are called also cause its set
                    // of upvars to be merged with the parent function's.
                    fb.upvars.unionWith(upvars);

                    if (join) {
                        join.intervene(binds, upvarInts);
                    } else {
                        binds.intervene(upvarInts);
                    }

                    return escaped;
                }

                //
                // Locally, direct evals can intervene between def and use,
                // i.e.  it can change existing bindings and introduce new
                // local ones, so blast away context.
                //
                var inners = this.binds.inners;
                var base = baseOfCall(n[0]);
                var target = targetOfCall(n[0], IDENTIFIER);

                if (target == "eval") {
                    escapeEval(join, binds);
                } else if (base.type == IDENTIFIER) {
                    var name = base.value;
                    var c = binds.current(name);
                    var uv;

                    if (!c && (uv = binds.upvar(name))) {
                        c = uv;

                        // We check for forward because if it's an upvar
                        // that's been forwarded, it got some immediate
                        // assignment like:
                        //
                        // function f() {
                        //   var x, z, h;
                        //   function g() {
                        //     z = h;
                        //     z();
                        //   }
                        // }
                        //
                        // Here we really only need to intervene on h, not z
                        // as well.
                        if (!base.forward) {
                            binds.addUpvar("intervenes", uv);
                        }
                    }

                    if (c) {
                        var upvars = c.upvars || new Upvars;
                        var escaped = processUpvars(upvars);

                        // Check its list of upvar uses to see if the same
                        // value has been maintained. This optimization is
                        // used for write-once upvar resolution:
                        //
                        // function f() {
                        //   var x;
                        //   function g() {
                        //     x; // should be forwarded to 0.
                        //   }
                        //
                        //   x = 0;
                        //   g();
                        // }
                        var uvuses = upvars.uses.members;
                        if (upvars.uses.length > 0) {
                            binds.addBackpatchUpvars(upvars);
                        }

                        for (var i in uvuses) {
                            var use = uvuses[i];
                            var name = use.name;

                            // If the upvar escaped, invalidate it.
                            if (fb.evalEscaped ||
                                (escaped && escaped.lookup(use))) {
                                upvars.uses.remove(use);
                                upvars.useTuples.remove(use);
                                continue;
                            }

                            if (binds.current(name) === use) {
                                // If the upvar is current, check if its value
                                // has changed since last invocation.
                                var useTuple = upvars.useTuples.lookup(use);
                                if (useTuple.cdef === undefined) {
                                    // If it's the first time, record the value.
                                    useTuple.cdef = use.def;
                                } else if (useTuple.cdef !== use.def) {
                                    // If it's changed, invalidate the use.
                                    upvars.uses.remove(use);
                                    upvars.useTuples.remove(use);
                                }
                            } else if (binds.upvar(name) === use) {
                                // If the upvar is still an upvar in the current
                                // scope and but hasn't been touched, we don't do
                                // anything. Otherwise we remove it from the list
                                // of uses to check.
                                if (hasOwnProperty.call(fb.upvarOlds, use.name)) {
                                    upvars.uses.remove(use);
                                    upvars.useTuples.remove(use);
                                }
                            }
                        }
                    }
                } else if (base.upvars) {
                    processUpvars(base.upvars);
                }

                var unionOnRight = n.type == CALL || n.type == NEW_WITH_ARGS ||
                                   n.type == INDEX;
                if (unionOnRight) {
                    escapeVars(join, binds, n[1].upvars || new Upvars);
                }
            }
        },

        PRIMARY: {
            finish: function(n) {
                if (n.type != IDENTIFIER)
                    return;

                var binds = this.binds, join = this.join;
                var fb = binds.nearestFunction;
                var name = n.value;
                var c = binds.current(name);
                var uv = binds.upvar(name);
                var hp = binds.hasParam(name);
                var hup = binds.hasUpParam(name);
                var mb = fb.mu(name);

                // Don't trust vars inside of withs.
                if (binds.isWith && c && c.type == VAR)
                    return;

                c = upvarTreatedAsLocal(binds, name, c, uv);

                if (c) {
                    n.setForward(c.def, uv === c);
                } else if (mb && (!uv && !hup && !hp)) {
                    // In the case where we're recursively calling ourself,
                    // remember the node for backpatching.
                    mb.pushMuUse(n);
                }

                /*
                 * Any identifier used in an inner block that is not already
                 * bound by a let _at the same block level_ is a possible let
                 * hoist.
                 */
                if (!this.secondPass && !binds.isFunction &&
                    !n.internal && !binds.currents[name]) {
                    binds.rememberPossibleHoist(name);
                }

                if (join) {
                    join.rememberUse(name, n);
                }

                //
                // If by flow the current node has upvars and is on the RHS
                // (either of an assignment or as an argument to new with
                // arguments or a call), we need to mark it as possibly
                // escaping.
                //
                if (binds.inRHS > 0) {
                    if (c && !uv) {
                        n.upvars = c.upvars;
                    } else if (uv) {
                        n.upvars = new Upvars;
                        n.upvars.flowIUVs.insert1(uv);
                    }
                }

                if (!c && !uv) {
                    if (!this.secondPass && !n.internal &&
                        !uv && !hp && !hup) {
                        //
                        // Any identifier that we don't know anything about is
                        // always at least a possible var hoist.
                        //
                        fb.rememberPossibleHoist(name);
                    }

                    binds.closureNeedsEscape();

                    var escaped = fb.escapedVars();
                    if (join) {
                        //
                        // This is annoying: we don't know if something is a
                        // direct eval until we finish parsing the MEMBER
                        // expression, so we can't insert a dash in the
                        // tryJoin here and have to do it there.
                        //
                        // Is this heuristic good enough?
                        //
                        var tryJoin = join.nearestTryJoin();
                        if (tryJoin && name != "eval") {
                            //
                            // We can be in a try, in which case throws modify
                            // the control flow. We don't kill the branch
                            // because we don't know if we're going to throw.
                            // We however need to propagate phis up to the try
                            // context and add a new branch.
                            //
                            var ps = join.unionPhisUpTo(tryJoin);
                            var tryBinds = tryJoin.binds;
                            var old;
                            for (var x in ps) {
                                c = tryBinds.current(x);
                                old = ps[x].old;
                                tryJoin.insertPhi(c.type, x, c.def, c.upvars,
                                                  old.def, old.upvars);
                            }
                            binds.closureNeedsEscape();
                            var intervened = join.intervene(binds, escaped);
                            tryJoin.insertDashes(binds, intervened);
                            tryJoin.finishBranch();
                            if (!tryJoin.isFinally) {
                                tryJoin.parent.maybeThrows = true;
                            }
                        } else {
                            join.intervene(binds, escaped);
                        }
                    } else {
                        binds.intervene(escaped);
                    }
                }
            }
        },

        PROPERTY_INIT: {
            build: function(t) {
                var n = new Node(t, PROPERTY_INIT);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            finish: function(n) {
                n.rhsUnionUpvars(n[1]);
            }
        },

        COMMA: {
            build: function(t) {
                var n = new Node(t, COMMA);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            addOperand: function(n, n2) {
                n.rhsUnionUpvars(n2);
                n.push(n2);
            }
        },

        LIST: {
            build: function(t) {
                var n = new Node(t, LIST);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            addOperand: function(n, n2) {
                n.rhsUnionUpvars(n2);
                n.push(n2);
            }
        },

        ARRAY_INIT: {
            build: function(t) {
                var n = new Node(t, ARRAY_INIT);
                n.rhsNewUpvars(this.binds);

                return n;
            },

            addElement: function(n, n2) {
                n.rhsUnionUpvars(n2);
                n.push(n2);
            }
        },

        OBJECT_INIT: {
            build: function(t) {
                var n = new Node(t, OBJECT_INIT);
                n.rhsNewUpvars(this.binds);
                return n;
            },

            addProperty: function(n, n2) {
                n.rhsUnionUpvars(n2);
                n.push(n2);
            }
        },

        WITH: {
            setObject: function(n, e) {
                // We need to record interventions for assignments
                n.object = e;
                // Clearing binds will kick the guard in and effectively
                // switch us back to the vanilla builder inside the eval. We
                // can't just intervene once and resume analysis on the next
                // assignment like for eval.
                this.binds = new Bindings(this.binds, false, true);
                this.binds.noPop = true;
                this.noBindingsOnNextBlock = true;
            },

            finish: function(n) {
                var binds = this.binds;
                var join = this.join;

                if (join) {
                    join.intervene(binds, binds.withIntervenes);
                } else {
                    binds.intervene(binds.withIntervenes);
                }
                this.binds = this.binds.parent;
            }
        },

        genDestructuringSym: function() {
            return "$dtmp" + this.destructuringTmpFresh++;
        },

        genPostfixSym: function() {
            return "$ptmp" + this.postfixTmpFresh++;
        },

        setHoists: function(id, vds) {
            this.hoists[id] = vds;
        }
    };

    extendBuilder(SSABuilder, parser.DefaultBuilder);

    /*
     * These methods are unguarded.
     */

    var Sbp = SSABuilder.prototype;

    Sbp.FUNCTION.build = function(t) {
        var binds = this.binds;
        var n = new Node(t);
        if (n.type != FUNCTION)
            n.type = (n.value == "get") ? GETTER : SETTER;
        n.params = [];

        // Save the current join, since we could be declaring an inner
        // function inside a branch.
        this.joinStack.push(this.join);
        this.join = null;
        this.binds = new Bindings(binds, true, false);
        this.binds.noPop = true;
        this.noBindingsOnNextBlock = true;

        return n;
    };

    /*
     * Utility functions.
     */

    function escapeEval(join, binds) {
        if (join) {
            var intervened = join.intervene(binds);
            var tryJoin = join.nearestTryJoin();
            if (tryJoin) {
                tryJoin.insertDashes(binds, intervened);
                tryJoin.finishBranch();
            }
        } else {
            binds.intervene();
        }

        // evals can make everything escape, all the time.
        binds.nearestFunction.evalEscaped = true;
        binds.closureIsEval();
    }

    function escapeVars0(join, binds, mayEscape, escapedIntervenes) {
        var fb = binds.nearestFunction;
        var escaped = fb.evalEscaped ? null : fb.escaped;

        binds.closureNeedsEscape();

        // We also need to remember if the function that
        // escaped calls other upvars:
        //
        // function f() {
        //   var x, y, h;
        //   function g() {
        //      h();
        //   }
        //   h = function () {
        //      x = 1;
        //   }
        //   globalCode(g);
        //   h = function () {
        //      y = 1;
        //   }
        //   otherGlobalCode() // should invalidate y
        if (escaped) {
            escaped.defs.unionWith(mayEscape);
            escaped.intervenes.unionWith(escapedIntervenes);
            etci = escaped.transClosureI(new Map);
        } else {
            etci = null;
        }

        if (join) {
            join.intervene(binds, etci);
        } else {
            binds.intervene(etci);
        }
    }

    function escapeVars(join, binds, upvars) {
        // Inner functions that escape cause its set of upvars to be unioned
        // with that of its parent.
        var fb = binds.nearestFunction;
        fb.upvars.unionWith(upvars);

        // Invalidate all upvar uses because the inner function escaped.
        upvars.uses.clear();
        upvars.useTuples.clear();

        if (upvars.isEval) {
            escapeEval(join, binds);
        } else {
            var tci;
            if (tci = upvars.transClosureI(new Map)) {
                escapeVars0(join, binds, tci, upvars.intervenes);
            } else {
                escapeEval(join, binds);
            }
        }

        var fiuvs = upvars.flowIUVs.members;
        for (var i in fiuvs) {
            binds.addUpvar("escapes", fiuvs[i])
        }
    }

    function unionPhiUpvars(operands) {
        var upvars = new Upvars;
        var puv, os;
        for (var i = 0, j = operands.length; i < j; i++) {
            os = operands[i];
            if (os) {
                if (puv = os.upvars) {
                    upvars.unionWith(puv);
                }
            }
        }

        return upvars;
    }

    function fallthroughJoin(n, join, binds) {
        var ps = join.phis, ps2 = {}, e, r;

        function go(x, rhs) {
            var upvars = unionPhiUpvars(ps[x]);
            binds.update(x, rhs, upvars);
        }

        //
        // Construct a new set of phis that have only two branches.
        //
        // The first branch is kept undefined and will be filled in with the
        // parent value (i.e. entering the branch via the case label and not by
        // falling through).
        //
        // The second branch copies over the current values (i.e. entering
        // the branch by falling through the previous case).
        //
        var c;
        for (var px in ps) {
            c = binds.current(px);
            ps2[px] = [];
            ps2[px][1] = { def: c.def, upvars: c.upvars };
            ps2[px].old = ps[px].old;
        }
        e = SSAJoin.mkJoin(ps2, join.parent, 2, go);
        r = new Node({ lineno: n.lineno }, SEMICOLON);
        r.expression = e;
        return r;
    }

    function baseOfCall(n) {
        switch (n.type) {
          case DOT:
            return baseOfCall(n[0]);
          case INDEX:
            return baseOfCall(n[0]);
          default:
            return n;
        }
    }

    function targetOfCall(n, ident) {
        switch (n.type) {
          case ident:
            return n.value;
          case DOT:
            return targetOfCall(n[1], IDENTIFIER);
          case INDEX:
            return targetOfCall(n[1], STRING);
          default:
            return null;
        }
    }

    function mkHoistDecl(builder, n) {
        var bDecl = builder.DECL;
        var hoistDecl = bDecl.build(n.tokenizer, n.type);

        bDecl.setName(hoistDecl, n.name);
        // Don't do another ASSIGN.
        hoistDecl.initializer = n.initializer;
        bDecl.setReadOnly(n, n.readOnly);
        bDecl.finish(hoistDecl);

        return hoistDecl;
    }

    function mkIndex(builder, t, rhs, x) {
        var bMember = builder.MEMBER;
        var bPrimary = builder.PRIMARY;
        var idx, idxLit;

        idxLit = bPrimary.build(t, STRING)
        idxLit.value = x;
        bPrimary.finish(idxLit);
        idx = bMember.build(t, INDEX);
        bMember.addOperand(idx, rhs);
        bMember.addOperand(idx, idxLit);
        bMember.finish(idx);

        return idx;
    }

    function mkIdentifier(builder, t, name, isExternal) {
        var bPrimary = builder.PRIMARY;
        var n = bPrimary.build(t, IDENTIFIER);
        n.name = n.value = name;
        n.internal = !isExternal;
        bPrimary.finish(n);

        return n;
    }

    function mkAssign(builder, t, lhs, initializer) {
        var bAssign = builder.ASSIGN;

        var n = bAssign.build(t);
        bAssign.addOperand(n, lhs);
        bAssign.addOperand(n, initializer);
        bAssign.finish(n);

        return n;
    }

    function mkAssignSimple(builder, t, name, initializer, isExternal) {
        var bPrimary = builder.PRIMARY;
        var id = bPrimary.build(t, IDENTIFIER);
        id.name = id.value = name;
        id.internal = !isExternal;
        bPrimary.finish(id);
        return mkAssign(builder, t, id, initializer);
    }

    function desugarDestructuringAssign(builder, comma, n, e) {
        var t = n.tokenizer;
        var binds = builder.binds;
        var bComma = builder.COMMA;

        function go(lhss, rhs) {
            var lhs, lhsType, idx;
            for (var x in lhss) {
                lhs = lhss[x];
                lhsType = lhs.type;
                idx = mkIndex(builder, t, rhs, x);
                if (lhsType == IDENTIFIER) {
                    var name = lhs.value;
                    var a = mkAssignSimple(builder, t, name, idx, true);
                    bComma.addOperand(comma, a);
                } else if (lhsType == ARRAY_INIT || lhsType == OBJECT_INIT) {
                    // We are not guaranteed simple names unlike in
                    // declarations.
                    go(lhs, idx);
                } else {
                    var a = mkAssign(builder, t, lhs, idx);
                    bComma.addOperand(comma, a);
                }
            }
        }

        // We need to the init to be linear.
        var decl = mkDecl(builder, "LET", n.tokenizer,
                          builder.genDestructuringSym(),
                          e, false);
        builder.binds.block.push(decl);
        decl[0].setForward(e);

        go(n.destructuredNames, decl[0]);
    }

    function desugarDestructuringInit(builder, n, e) {
        var t = n.tokenizer;
        var binds = builder.binds;
        var ddecls = n.destructuredDecls;
        var bDecl = builder.DECL;

        function go(ddecls, rhs) {
            var n2, n3, id, idxLit, idx;
            for (var x in ddecls) {
                n2 = ddecls[x];

                // Desugar to an index operation.
                idx = mkIndex(builder, t, rhs, x);

                if (n2.type == IDENTIFIER) {
                    bDecl.setInitializer(n2, idx);
                    bDecl.finish(n2);
                } else {
                    go(n2, idx);
                }
            }
        }

        // We need the right hand side of the destructuring assignment
        // to be linear after the transform, so we first need to
        // assign the right hand side to a separate decl.
        var block = builder.binds.block;
        if (block) {
            var dtmp = builder.genDestructuringSym();
            var decl = mkDecl(builder, "LET", t, dtmp, e, false);
            block.push(decl);
            decl[0].setForward(e);
            go(ddecls, decl[0]);
        } else {
            // This only happens when we have destructuring for a catch var,
            // in which case that catch var already has let-scoping, so we
            // don't need to make a new let decl.
            go(ddecls, e);
        }

    }

    function mkAddDestructuringDecl(type, ro) {
        return function(n, n2, x) {
            var t = n2.tokenizer;
            var binds = this.binds;
            var bDecl = this.DECL;
            var numDecls = 0;
            var b;
            if (type == VAR) {
                b = this.VAR;
            } else if (type == CONST) {
                b = this.CONST;
            } else {
                b = this.LET;
            }

            function go(lhss) {
                var ddecls = {};

                for (var idx in lhss) {
                    var lhs = lhss[idx];
                    if (lhs.type == IDENTIFIER) {
                        var decl = bDecl.build(t);
                        var name = lhs.value;
                        decl.forward = lhs.forward;

                        // In declarations we're guaranteed that the
                        // pattern contains only simple names.
                        bDecl.setName(decl, name);
                        bDecl.setReadOnly(decl, ro);
                        // For decrementing localUses in
                        // desugarDestructuringInit.
                        decl.wasLocal = binds.hasCurrent(name);
                        b.addDecl(n, decl, x);
                        ddecls[idx] = decl;
                        numDecls++;
                    } else {
                        ddecls[idx] = go(lhs);
                    }
                }

                return ddecls;
            }

            n2.destructuredDecls = go(n2.name.destructuredNames);
            n2.numDecls = numDecls;
        }
    }

    function mkDecl(builder, tt, t, name, initializer, isExternal) {
        var b = builder[tt];
        var bDecl = builder.DECL;

        var decl = bDecl.build(t);
        bDecl.setName(decl, name);
        decl.internal = !isExternal;
        var lt = b.build(t);
        b.addDecl(lt, decl);

        if (initializer) {
            bDecl.setInitializer(decl, initializer);
        }

        bDecl.finish(decl);
        b.finish(lt);

        return lt;
    }

    function mkCall(f, n, isExternal) {
        var ident = new Node(n.tokenizer, IDENTIFIER);
        ident.value = f;
        ident.internal = !isExternal;
        return new Node(n.tokenizer, CALL, ident,
                        new Node(n.tokenizer, LIST, n));
    }

    function mkRawIdentifier(t, name, init, isExternal) {
        var n = new Node(t, IDENTIFIER);
        n.name = n.value = name;
        n.internal = !isExternal;
        n.initializer = init;
        return n;
    }

    function mkNumber(t, i, isExternal) {
        var n = new Node(t, NUMBER);
        n.value = i;
        n.internal = !isExternal;
        return n;
    }

    var Np = Node.prototype;

    Np.rhsNewUpvars = function(binds) {
        if (binds.inRHS > 0) {
            this.upvars = new Upvars;
        }
    };

    Np.rhsUnionUpvars = function(n) {
        if (this.upvars && n.upvars) {
            this.upvars.unionWith(n.upvars)
        }
    };

    function upvarTreatedAsLocal(binds, name, c, uv) {
        var fb = binds.nearestFunction;
        // If we have a backup, then we've assigned to this upvar
        // in the current function and treat it as a local.
        return hasOwnProperty.call(fb.upvarOlds, name) ? uv : c;
    }

    Np.setForward = function(fwd, isUpvar) {
        // Don't link to intervened.
        if (fwd === dash)
            return;

        this.forward = fwd;
        if (fwd) {
            if (!fwd.backwards)
                fwd.backwards = [];

            fwd.backwards.push(this);
        }
    };

    Np.resolve = function() {
        var f = this.forward;
        if (f) {
            this.forward = f.resolve();
            return this.forward;
        }
        return this;
    };

    // Push a pointer to a phi node to a node so that the latter fill in phi
    // node's value when it itself is computed.
    Np.pushPhiUse = function(p) {
        if (!this.phiUses)
            this.phiUses = [];

        this.phiUses.push(p);
    };

    parser.SSABuilder = SSABuilder;

}());
