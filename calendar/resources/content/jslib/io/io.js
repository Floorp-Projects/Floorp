/*** -*- Mode: Javascript; tab-width: 2;

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Collabnet code.
The Initial Developer of the Original Code is Collabnet.

Portions created by Collabnet are
Copyright (C) 2000 Collabnet.  All
Rights Reserved.

Contributor(s): Pete Collins, Doug Turner, Brendan Eich, Warren Harris

***/

if(typeof(JS_LIB_LOADED)=='boolean')
{
/********************* INCLUDED FILES **************/
include(JS_LIB_PATH+'io/filesystem.js');
include(JS_LIB_PATH+'io/file.js');
include(JS_LIB_PATH+'io/dir.js');
include(JS_LIB_PATH+'io/fileUtils.js');
include(JS_LIB_PATH+'io/dirUtils.js');
/********************* INCLUDED FILES **************/
}

else
{
    dump("JSLIB library not loaded:\n"                                  +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/io/io.js');\n\n");
}

