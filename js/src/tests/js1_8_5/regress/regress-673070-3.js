// |reftest| pref(javascript.options.xml.content,true)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

let (x = 1) {
    let ([] = [<x/>], r = <x/>) {}
}

reportCompare(0, 0, 'ok');
