/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 449666;
var summary = 'Do not assert: JSSTRING_IS_FLAT(str_)';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var global;


  if (typeof window == 'undefined') {
    global = this;
  }
  else {
    global = window;
  }

  if (!global['g']) {
    global['g'] = {};
  }

  if (!global['g']['l']) {
    global['g']['l'] = {};
    (function() {
      function k(a,b){
        var c=a.split(/\./);
        var d=global;
        for(var e=0;e<c.length-1;e++){
          if(!d[c[e]]){
            d[c[e]]={};
          }
          d=d[c[e]];
        }
        d[c[c.length-1]]=b;
        print("hi");
      }

      function T(a){return "hmm"}
      k("g.l.loaded",T);
    })();

  }


  reportCompare(expect, actual, summary);
}
