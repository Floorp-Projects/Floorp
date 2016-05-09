/* 
 * Ensure that flat matches with metachars in them don't have their metachars
 * interpreted as special.
 */

function isPatternSyntaxError(pattern) {
    try {
        new RegExp(pattern);
        return false;
    } catch (e if e instanceof SyntaxError) {
        return true;
    }
}

// Bug example.
assertEq("1+2".replace("1+2", "$&+3"), "1+2+3");
assertEq("1112".replace("1+2", ""), "1112");

// ^
assertEq("leading text^my hat".replace("^my hat", ""), "leading text");
assertEq("my hat".replace("^my hat", ""), "my hat");

// $
assertEq("leading text$my money".replace("leading text$", ""), "my money");
assertEq("leading text".replace("leading text$", ""), "leading text");

// \
var BSL = '\\';
assertEq(("dir C:" + BSL + "Windoze").replace("C:" + BSL, ""),
         "dir Windoze");
assertEq(isPatternSyntaxError("C:" + BSL), true);

// .
assertEq("This is a sentence. It has words.".replace(".", "!"),
         "This is a sentence! It has words.");
assertEq("This is an unterminated sentence".replace(".", ""),
         "This is an unterminated sentence");

// *
assertEq("Video killed the radio *".replace(" *", ""), "Video killed the radio");
assertEq("aaa".replace("a*", ""), "aaa");

// +
assertEq("On the + side".replace(" +", ""), "On the side");
assertEq("1111111111111".replace("1+", ""), "1111111111111");

// \+
assertEq(("Inverse cone head: " + BSL + "++/").replace(BSL + "+", ""),
         "Inverse cone head: +/");
assertEq((BSL + BSL + BSL).replace(BSL + "+", ""),
         BSL + BSL + BSL);

// \\+
assertEq((BSL + BSL + "+").replace(BSL + BSL + "+", ""), "");
assertEq((BSL + BSL + BSL).replace(BSL + BSL + "+", ""), (BSL + BSL + BSL));

// \\\
assertEq((BSL + BSL + BSL + BSL).replace(BSL + BSL + BSL, ""), BSL);
assertEq(isPatternSyntaxError(BSL + BSL + BSL), true);

// \\\\
assertEq((BSL + BSL + BSL + BSL).replace(BSL + BSL + BSL + BSL, "", "i"), "");
assertEq((BSL + BSL).replace(BSL + BSL + BSL + BSL, ""), BSL + BSL);


// ?
assertEq("Pressing question?".replace("?", "."), "Pressing question.");
assertEq("a".replace("a?", ""), "a");

// (
assertEq("(a".replace("(", ""), "a");

// )
assertEq("a)".replace(")", ""), "a");

// ( and )
assertEq("(foo)".replace("(foo)", ""), "");
assertEq("a".replace("(a)", ""), "a");

// [
assertEq("[a".replace("[", ""), "a");

// ]
assertEq("a]".replace("]", ""), "a");

// [ and ]
assertEq("a".replace("[a-z]", ""), "a");
assertEq("You would write your regexp as [a-z]".replace("[a-z]", ""),
         "You would write your regexp as ");

// {
assertEq("Numbers may be specified in the interval {1,100}".replace("{1,", ""),
         "Numbers may be specified in the interval 100}");

// }
assertEq("Numbers may be specified in the interval {1,100}".replace(",100}", ""),
         "Numbers may be specified in the interval {1");

// { and }
assertEq("Numbers may be specified in the interval {1,100}".replace(" {1,100}", ""),
         "Numbers may be specified in the interval");
assertEq("aaa".replace("a{1,10}", ""), "aaa");

// |
assertEq("Mr. Gorbachev|Tear down this wall!".replace("|Tear down this wall!", ""),
         "Mr. Gorbachev");
assertEq("foobar".replace("foo|bar", ""), "foobar");

print("PASS");
