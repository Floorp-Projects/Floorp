/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Nicholas Nethercote
 */
gTestfile = 'regress-563210';

if (typeof gczeal != 'undefined' && typeof gc != 'undefined') {
    try
    {
        try {
            __defineGetter__("x", gc)
        } catch (e) {}
        gczeal(1)
        print(x)(Array(-8))
    }
    catch(ex)
    {
    }
}
reportCompare("no assertion failure", "no assertion failure", "bug 563210");


