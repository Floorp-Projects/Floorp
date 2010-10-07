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
assertEq("1+2".replace("1+2", "$&+3", "g"), "1+2+3");
assertEq("1112".replace("1+2", "", "g"), "1112");

// ^
assertEq("leading text^my hat".replace("^my hat", "", "g"), "leading text");
assertEq("my hat".replace("^my hat", "", "g"), "my hat");

// $
assertEq("leading text$my money".replace("leading text$", "", "g"), "my money");
assertEq("leading text".replace("leading text$", "", "g"), "leading text");

// \
var BSL = '\\';
assertEq(("dir C:" + BSL + "Windoze").replace("C:" + BSL, "", "g"),
         "dir Windoze");
assertEq(isPatternSyntaxError("C:" + BSL), true);

// .
assertEq("This is a sentence. It has words.".replace(".", "!", "g"),
         "This is a sentence! It has words!");
assertEq("This is an unterminated sentence".replace(".", "", "g"),
         "This is an unterminated sentence");

// *
assertEq("Video killed the radio *".replace(" *", "", "g"), "Video killed the radio");
assertEq("aaa".replace("a*", "", "g"), "aaa");

// +
assertEq("On the + side".replace(" +", "", "g"), "On the side");
assertEq("1111111111111".replace("1+", "", "g"), "1111111111111");

// \+
assertEq(("Inverse cone head: " + BSL + "++/").replace(BSL + "+", "", "g"),
         "Inverse cone head: +/");
assertEq((BSL + BSL + BSL).replace(BSL + "+", "", "g"),
         BSL + BSL + BSL);

// \\+
assertEq((BSL + BSL + "+").replace(BSL + BSL + "+", "", "g"), "");
assertEq((BSL + BSL + BSL).replace(BSL + BSL + "+", "", "g"), (BSL + BSL + BSL));

// \\\
assertEq((BSL + BSL + BSL + BSL).replace(BSL + BSL + BSL, "", "g"), BSL);
assertEq(isPatternSyntaxError(BSL + BSL + BSL), true);

// \\\\
assertEq((BSL + BSL + BSL + BSL).replace(BSL + BSL + BSL + BSL, "", "i"), "");
assertEq((BSL + BSL).replace(BSL + BSL + BSL + BSL, "", "g"), BSL + BSL);


// ?
assertEq("Pressing question?".replace("?", ".", "g"), "Pressing question.");
assertEq("a".replace("a?", "", "g"), "a");

// (
assertEq("(a".replace("(", "", "g"), "a");

// )
assertEq("a)".replace(")", "", "g"), "a");

// ( and )
assertEq("(foo)".replace("(foo)", "", "g"), "");
assertEq("a".replace("(a)", "", "g"), "a");

// [
assertEq("[a".replace("[", "", "g"), "a");

// ]
assertEq("a]".replace("]", "", "g"), "a");

// [ and ]
assertEq("a".replace("[a-z]", "", "g"), "a");
assertEq("You would write your regexp as [a-z]".replace("[a-z]", "", "g"),
         "You would write your regexp as ");

// {
assertEq("Numbers may be specified in the interval {1,100}".replace("{1,", "", "g"),
         "Numbers may be specified in the interval 100}");

// }
assertEq("Numbers may be specified in the interval {1,100}".replace(",100}", "", "g"),
         "Numbers may be specified in the interval {1");

// { and }
assertEq("Numbers may be specified in the interval {1,100}".replace(" {1,100}", "", "g"),
         "Numbers may be specified in the interval");
assertEq("aaa".replace("a{1,10}", "", "g"), "aaa");

// |
assertEq("Mr. Gorbachev|Tear down this wall!".replace("|Tear down this wall!", "", "g"),
         "Mr. Gorbachev");
assertEq("foobar".replace("foo|bar", "", "g"), "foobar");

print("PASS");
