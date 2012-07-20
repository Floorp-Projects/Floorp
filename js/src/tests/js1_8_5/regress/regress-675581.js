// |reftest| pref(javascript.options.xml.content,true)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

x=<x/>
x.(-0 in x)

reportCompare(0, 0, 'ok');
