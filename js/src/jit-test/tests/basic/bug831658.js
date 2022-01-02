// Don't assert.
String.prototype.search = evalcx('').String.prototype.search
x = /./.test()
''.search(/()/)
