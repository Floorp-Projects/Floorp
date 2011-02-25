/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Jason Orendorff
 */

var x = Proxy.create({
    iterate: function () {
            function f(){}
            f.next = function () { throw StopIteration; }
            return f;
        }
    });

for each (var e in [{}, {}, {}, {}, {}, {}, {}, {}, x]) {
    for (var v in e)  // do not assert
        ;
}

reportCompare(0, 0, 'ok');
