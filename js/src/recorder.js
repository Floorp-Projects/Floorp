/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Andreas Gal <gal@uci.edu>
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
({
    start: function(pc, sp) {
        this.error = false;
        this.anchorPC = pc;
        this.anchorSP = this.SP = sp;
        this.code = [];
        this.map = {};
        print("Recording at @" + pc);
    },
    stop: function(pc) {
        print("Recording failed at @" + pc);
    },
    /* track the data flow through locals */
    track: function(from, to) {
        this.map[to] = this.map[from];
    },
    /* register a change in the stack pointer */
    setSP: function(sp) {
        this.SP = sp;
    },
    /* emit an IR instruction */
    emit: function(x, into) {
        return this.code.push(x);
    },
    nullary: function(op, into, value) {
        print("nullary=" + op + " into=" + into + " value=" + value);
        var x = this.emit({ op: op, value: value});
        if (into)
            this.map[into] = x;
        return x;
    },
    unary: function(op, operand, into, value) {
        print("unary=" + op + " operand=" + operand + " into=" + into + " value=" + value);
        var x = this.emit({ op: op, value: value, operand: operand });
        if (into)
            this.map[into] = x;
        return x;
    },
    binary: function(op, left, right, into, value) {
        print("binary=" + op + " left=" + left + " right=" + right + " into=" + into + " value=" + value);
        var x = this.emit({ op: op, value: value, left: this.map[left], right: this.map[right] });
        if (into)
            this.map[into] = x;
        return x;
    },
    /* create a constant and assign it to v */
    generate_constant: function(v, vv) {
        this.nullary("generate_constant", v, vv);
    },
    boolean_to_jsval: function(b, v, vv) {
        this.unary("boolean_to_jsval", b, v, vv);
    },
    string_to_jsval: function(s, v, vv) {
        this.unary("string_to_jsval", s, v, vv);
    },    
    object_to_jsval: function(o, v, vv) {
        this.unary("object_to_jsval", o, v, vv);
    },
    id_to_jsval: function(id, v, vv) {
        this.unary("id_to_jsval", id, v, vv);
    },
    guard_jsdouble_is_int_and_int_fits_in_jsval: function(d, i, vv, g) {
        this.unary("guard_jsdouble_is_int_and_fits_in_jsval", d, i, vv).guard = g;
    },
    int_to_jsval: function(i, v, vv) {
        this.unary("int_to_jsval", i, v, vv);
    },
    new_double_in_rooted_value: function(d, v, vv) {
        this.unary("new_double_in_rooted_value", d, v, vv);
    },
    guard_int_fits_in_jsval: function(i, g) {
        this.unary("guard_int_fits_in_jsval", i).guard = g;
    },
    int_to_double: function(i, d, vv) {
        this.unary("int_to_double", i, d, vv);
    },
    guard_uint_fits_in_jsval: function(u, g) {
        this.unary("guard_uint_fits_in_jsval", u).guard = g;
    },
    uint_to_jsval: function(u, v, vv) {
        this.unary(u, v, vv);
    },
    uint_to_double: function(u, d, vv) {
        this.unary("uint_to_double", u, d, vv);
    },
    guard_jsval_is_int: function(v, g) {
        this.unary("guard_jsval_is_int", v).guard = g;
    },
    jsval_to_int: function(v, i, vv) {
        this.unary("jsval_to_int", v, i, vv);
    },
    guard_jsval_is_double: function(v, g) {
        this.unary("guard_jsval_is_double", v).guard = g;
    },
    jsval_to_double: function(v, d, vv) {
        this.unary("jsval_to_double", v, d, vv);
    },
    ValueToNumber: function(v, d, vv) {
        this.unary("ValueToNumber", v, d, vv);
    },
    guard_jsval_is_null: function(v, g) {
        this.unary("guard_jsval_is_null", v).guard = g;
    },
    ValueToECMAInt32: function(v, i, vv) {
        this.unary("ValueToECMAInt32", v, i, vv);
    },
    int_to_uint: function(i, u, vv) {
        this.unary("int_to_uint", i, u, vv);
    },
    ValueToECMAUint32: function(v, u, vv) {
        this.unary("ValueToECMAUint32", v, u, vv);
    },
    generate_boolean_constant: function(b, vv) {
        this.nullary("boolean_constant", b, vv);
    },
    guard_jsval_is_boolean: function(v, g) {
        this.unary("guard_jsval_is_boolean", v).guard = g;
    },
    jsval_to_boolean: function(v, b, vv) {
        this.unary("jsval_to_boolean", v, b, vv);
    },
    ValueToBoolean: function(v, b, vv) {
        this.unary("ValueToBoolean", v, b, vv);
    },
    guard_jsval_is_primitive: function(v, g) {
        this.unary("guard_jsval_is_primitive", v).guard = g;
    },
    jsval_to_object: function(v, obj, vv) {
        this.unary("jsval_to_object", v, obj, vv);
    },
    guard_obj_is_null: function(obj, g) {
        this.unary("guard_obj_is_null", obj).guard = g;
    },
    ValueToNonNullObject: function(v, obj, vv) {
        this.unary("ValueToNonNullObject", v, obj, vv);
    },
    call_obj_default_value: function(obj, hint, v, vv) {
        this.binary("obj_to_default_value", obj, hint, v, vv);
    },
    dadd: function(a, b, r, vv) {
        this.binary("dadd", a, b, r, vv);
    },
    dsub: function(a, b, r, vv) {
        this.binary("dsub", a, b, r, vv);
    },
    dmul: function(a, b, r, vv) {
        this.binary("dmul", a, b, r, vv);
    },
    ior: function(a, b, r, vv) {
        this.binary("ior", a, b, r, vv);
    },
    ixor: function(a, b, r, vv) {
        this.binary("ixor", a, b, r, vv);
    },
    iand: function(a, b, r, vv) {
        this.binary("iand", a, b, r, vv);
    },
    ilsh: function(a, b, r, vv) {
        this.binary("ilsh", a, b, r, vv);
    },
    irsh: function(a, b, r, vv) {
        this.binary("irsh", a, b, r, vv);
    },
    ursh: function(a, b, r, vv) {
        this.binary("ursh", a, b, r, vv);
    },
    guard_boolean_is_true: function(b, g) {
        this.unary("guard_boolean_is_true", b).guard = g;
    },
    icmp_lt: function(a, b,     r, vv) {
        this.binary("icmp_lt", a, b, r, vv);
    },
    icmp_le: function(a, b, r, vv) {
        this.binary("icmp_le", a, b, r, vv);
    },
    icmp_gt: function(a, b, r, vv) {
        this.binary("icmp_gt", a, b, r, vv);
    },
    icmp_ge: function(a, b, r, vv) {
        this.binary("icmp_ge", a, b, r, vv);
    },
    dcmp_lt: function(a, b, r, vv) {
        this.binary("dcmp_lt", a, b, r, vv);
    },
    dcmp_le: function(a, b, r, vv) {
        this.binary("dcmp_le", a, b, r, vv);
    },
    dcmp_gt: function(a, b, r, vv) {
        this.binary("dcmp_gt", a, b, r, vv);
    },
    dcmp_ge: function(a, b, r, vv) {
        this.binary("dcmp_ge", a, b, r, vv);
    },
    dcmp_lt_ifnan: function(a, b, r, vv) {
        this.binary("dcmp_lt", a, b, r, vv).if_nan = true;
    },
    dcmp_le_ifnan: function(a, b, r, vv) {
        this.binary("dcmp_le", a, b, r, vv).if_nan = true;
    },
    dcmp_gt_ifnan: function(a, b, r, vv) {
        this.binary("dcmp_gt", a, b, r, vv).if_nan = true;
    },
    dcmp_ge_ifnan: function(a, b, r, vv) {
        this.binary("dcmp_ge", a, b, r, vv).if_nan = true;
    },
    generate_int_constant: function(c, vv) {
        this.nullary("generate_int_constant", c, vv);
    },
    jsval_to_string: function(v, s, vv) {
        this.unary("jsval_to_string", v, s, vv);
    },
    CompareStrings: function(a, b, r, vv) {
        this.binary("CompareStrings", a, b, r, vv);
    },
    guard_both_jsvals_are_int: function(a, b, g) {
        this.binary("guard_both_jsvals_are_int", a, b).guard = g;
    },
    guard_both_jsvals_are_strings: function(a, b, g) {
        this.binary("guard_both_jsvals_are_strings", a, b).guard = g;
    },
    guard_can_do_fast_inc_dec: function(v, g) {
        this.unary("guard_can_do_fast_inc_dec", v).guard = g;
    }
});
