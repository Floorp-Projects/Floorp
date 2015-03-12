/*
The code in this file is adapted from the original
Library Detector add-on
(https://addons.mozilla.org/en-US/firefox/addon/library-detector/) written by
Paul Bakaus (http://paulbakaus.com/) and made available under the
MIT License (http://www.opensource.org/licenses/mit-license.php).
*/

var LD_tests = {

    'jQuery': {
        test: function(win) {
            var jq = win.jQuery || win.$ || win.$jq || win.$j;
            if(jq && jq.fn && jq.fn.jquery) {
                return { version: jq.fn.jquery };
            } else {
                return false;
            }
        }
    },

    'jQuery UI': {
        //phonehome: 'http://jqueryui.com/phone_home',
        test: function(win) {

            var jq = win.jQuery || win.$ || win.$jq || win.$j;
            if(jq && jq.fn && jq.fn.jquery && jq.ui) {

                var plugins = 'accordion,datepicker,dialog,draggable,droppable,progressbar,resizable,selectable,slider,menu,grid,tabs'.split(','), concat = [];
                for (var i=0; i < plugins.length; i++) { if(jq.ui[plugins[i]]) concat.push(plugins[i].substr(0,1).toUpperCase() + plugins[i].substr(1)); };

                return { version: jq.ui.version, details: concat.length ? 'Plugins used: '+concat.join(',') : '' };
            } else {
                return false;
            }

        }
    },

    'MooTools': {
        test: function(win) {
            if(win.MooTools && win.MooTools.version) {
                return { version: win.MooTools.version };
            } else {
                return false;
            }
        }
    },

    'YUI': {
        test: function(win) {
            if(win.YAHOO && win.YAHOO.VERSION) {
                return { version: win.YAHOO.VERSION };
            } else {
                return false;
            }
        }
    },

    'Closure': {
        test: function(win) {
            if(win.goog) {
                return { version: '2.0' };
            }
            return false;
        }
    },

    'Modernizr': {
        test: function(win) {
            if(win.Modernizr) {
                return { version: win.Modernizr._version };
            }
            return false;
        }
    },


};

function testLibraries() {
  var win = unsafeWindow;
  var libraryList = [];
  for(var i in LD_tests) {
    var passed = LD_tests[i].test(win);
    if (passed) {
      let libraryInfo = {
        name: i,
        version: passed.version
      };
      libraryList.push(libraryInfo);
    }
  }
  self.postMessage(libraryList);
}

testLibraries();