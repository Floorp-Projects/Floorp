/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
({
  loc:{start:{line:1, column:0}, end:{line:1, column:12}, source:null},
  type:"Program",
  body:[
    {
      loc:{start:{line:1, column:0}, end:{line:1, column:12}, source:null},
      type:"ExpressionStatement",
      expression:{
        loc:{start:{line:1, column:0}, end:{line:1, column:12}, source:null},
        type:"Literal",
        value:"use strict"
      }
    }
  ]
})
*/

function run_test() {
  // Reflect.parse is better tested in js shell; this basically tests its presence.
  var parseData = Reflect.parse('"use strict"');
  Assert.equal(parseData.body[0].expression.value, "use strict");
}
