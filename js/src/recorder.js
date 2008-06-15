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
        print("Recording ended at @" + pc);
    },
    /* track the data flow through locals */
    track: function(from, to) {
        this.map[to] = this.map[from];
    },
    /* emit an IR instruction */
    emit: function(x, to) {
        this.map[to] = this.code.push(x);
    },
    /* register a change in the stack pointer */
    setSP: function(sp) {
        this.SP = sp;
    },
    /* create a constant and assign it to v */
    generate_constant: function(v, vv) {
        this.emit({ op: "generate_constant", value: vv });
    },
    boolean_to_jsval: function(b, v, vv) {
        this.emit({ op: "boolean_to_jsval", value: vv, operand: this.map[b] });
    },
    string_to_jsval: function(s, v, vv) {
        this.emit({ op: "string_to_jsval", value: vv, operand: this.map[s] });
    },
    object_to_jsval: function(o, v, vv) {
        this.emit({ op: "object_to_jsval", value: vv, operand: this.map[o] });
    },
    id_to_jsval: function(id, v, vv) {
        this.emit({ op: "id_to_jsval", value: vv, operand: this.map[id] });
    },
    guard_jsdouble_is_int_and_int_fits_in_jsval: function(d, i, vv, g) {
        this.emit({ op: "guard_jsdouble_is_int_and_int_fits_in_jsval", value: vv, 
                    operand: this.map[i], state: g });
    }
});
