NODE PROPERTIES UPGRADING

Original library at https://github.com/gagle/node-properties
The original library is intended for node and not for the browser. Most files are not
needed here.

To update
- copy https://github.com/gagle/node-properties/blob/master/lib/parse.js
- update the initial "module.exports" to "var parse" in parse.js
- copy https://github.com/gagle/node-properties/blob/master/lib/read.js
- remove the require statements at the beginning
- merge the two files, parse.js first, read.js second