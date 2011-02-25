load(libdir + 'eqArrayHelper.js');
assertEqArray(/(?:(?:(")(c)")?)*/.exec('"c"'), [ '"c"', '"', "c" ]);
assertEqArray(/(?:(?:a*?(")(c)")?)*/.exec('"c"'), [ '"c"', '"', "c" ]);
assertEqArray(/<script\s*(?![^>]*type=['"]?(?:dojo\/|text\/html\b))(?:[^>]*?(?:src=(['"]?)([^>]*?)\1[^>]*)?)*>([\s\S]*?)<\/script>/gi.exec('<script type="text/javascript" src="..."></script>'), ['<script type="text/javascript" src="..."></script>', '"', "...", ""]);
