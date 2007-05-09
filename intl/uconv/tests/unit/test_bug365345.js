/* Tests conversion of unrepresented characters that should be transliterated
 * to spaces (bug 365345), and some others from transliterate.properties while
 * I'm here
 */

const inSpace = "Hello Space";
const inEnSpace = "Hello\u2002EnSpace";
const inEmSpace = "Hello\u2003EmSpace";
const inEuro = "Hello\u20ACEuro";
const inTamil1000 = "Hello\u0BF2Tamil1000";
const inMonospace9 = "Hello\ud835\udfffMonospace9";
    
const expectedSpace = "Hello Space";
const expectedEnSpace = "Hello EnSpace";
const expectedEmSpace = "Hello EmSpace";
const expectedEuro = "HelloEUREuro";
const expectedTamil1000 = "Hello[1000]Tamil1000";
const expectedMonospace9 = "Hello9Monospace9";

const EntityAfterCharsetConv = 512;
const transliterate = 8;

const charset = "ISO-8859-1";
    
function run_test() {
    var SaveAsCharset =
	Components.Constructor("@mozilla.org/intl/saveascharset;1",
			       "nsISaveAsCharset",
			       "Init");

    var converter = new SaveAsCharset(charset,
				      EntityAfterCharsetConv, 
				      transliterate);

    var outSpace = converter.Convert(inSpace);
    do_check_eq(outSpace, expectedSpace);

    var outEnSpace = converter.Convert(inEnSpace);
    do_check_eq(outEnSpace, expectedEnSpace);

    var outEmSpace = converter.Convert(inEmSpace);
    do_check_eq(outEmSpace, expectedEmSpace);

    var outEuro = converter.Convert(inEuro);
    do_check_eq(outEuro, expectedEuro);

    var outTamil1000 = converter.Convert(inTamil1000);
    do_check_eq(outTamil1000, expectedTamil1000);

    var outMonospace9 = converter.Convert(inMonospace9);
    do_check_eq(outMonospace9, expectedMonospace9);
}
