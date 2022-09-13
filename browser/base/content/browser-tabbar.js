/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 setTabbarMode();
 
//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------

 Services.prefs.addObserver("floorp.browser.tabbar.settings", function(){
   try {
    document.getElementById("tabbardesgin").remove();
    document.getElementById("tabbar-script").remove();
   } catch(e) {}
   document.getElementById("navigator-toolbox").insertBefore(document.getElementById("titlebar"), document.getElementById("navigator-toolbox").firstChild);
   document.getElementById("TabsToolbar").before(document.getElementById("toolbar-menubar"));

   setTabbarMode();
  }
 )