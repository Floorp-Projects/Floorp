// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

if (typeof disassemble != 'undefined')
{
    var func = disassemble(function() { return "c\\d"; })

    // The disassembled function will contain a bytecode "string" with the content of the string next to it.
    // Check if that string isn't over-escaped i.e. \\ isn't escaped to \\\\ .
    assertEq(func.indexOf("\\\\\\\\"), -1)
}

reportCompare(0, 0, 'ok');
