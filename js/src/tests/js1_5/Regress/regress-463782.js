/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 463782;
var summary = 'Do not assert: "need a way to EOT now, since this is trace end": 0';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

function dateCheck() {
  return true;
}
function dateToString()
{
  if (!this.dtsReturnValue)
    this.dtsReturnValue = "200811080616";
  return this.dtsReturnValue
    }
      
function placeAd2() {
  var adClasses = {
    "": {
    templateCheck: function () {
        var foo = ({
          allianz:{
            where:["intl/turningpoints"],
                when:["200805010000/200901010000"],
                what:["!234x60", "!bigbox_2", "!leaderboard_2", "!88x31"]
                },
              trendMicro:{
            where:["techbiz/tech/threatmeter"],
                when:["200806110000/200812310000"],
                what:["leaderboard"]
                },
              rolex_bb:{
            where:["politics/transitions"],
                when:["200811050000/200901312359"],
                what:["!bigbox"]
                }
          });
        
        for (a in foo) {
          if (dateCheck("", dateToString())) {
            for (var c = 0; c < 1; c++) {
            }
          }
        }
        return true;
      }
    }
  };
      
  adClasses[""].templateCheck();
}

placeAd2();

jit(false);

reportCompare(expect, actual, summary);

