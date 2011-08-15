// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

with (0)
    for (var b = 0 in 0)  // don't assert in parser
	;

reportCompare(0, 0, 'ok');
