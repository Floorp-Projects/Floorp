// don't crash
var book = 'Ps';
var pattern =   "(?:"
+                   "(?:"
+                       "(?:"
+                           "(?:-|)"
+                           "\\s?"
+                       ")"
+                       "|"
+                   ")"
+                   " ?"
+                   "\\d+"
+                   "\\w?"
+               ")*";
var re = new RegExp(pattern);
'8:5-8'.match(re);
