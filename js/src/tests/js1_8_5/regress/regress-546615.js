// |reftest| pref(javascript.options.xml.content,true)
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Jason Orendorff
 */

try {
    <y a={0
           }/>;/*7890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901*/x(x
        for(x in z));
} catch (exc) {}

reportCompare("no crash", "no crash", "Don't crash due to incorrect column numbers in long lines starting in XML.");
