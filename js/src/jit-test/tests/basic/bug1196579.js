
setJitCompilerOption("ion.warmup.trigger", 30);
var spaces = [ 
"\u0009", "\u000b", "\u000c", "\u0020", "\u00a0", "\u1680",
"\u180e", "\u2000", "\u2001", "\u2002", "\u2003", "\u2004",
"\u2005", "\u2006", "\u2007", "\u2008", "\u2009", "\u200a",
];
var line_terminators = [ "\u2028", "\u2029", "\u000a", "\u000d" ];
var space_chars = [].concat(spaces, line_terminators);
var non_space_chars = [ "\u200b", "\u200c", "\u200d" ];
var chars = [].concat(space_chars, non_space_chars);
var is_space = [].concat(space_chars.map(function(ch) { return true; }),
non_space_chars.map(function() { return false; }));
chars.map(function(ch) {}).join(',');
