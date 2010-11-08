/* Tests transliteration of new characters in Unicode 5.1, 5.2, and 6.0
 */

const inTeluguFractions = "\u0C78\u0C79\u0C7A\u0C7B\u0C7C\u0C7D\u0C7E";
const inMalayalamNumbers = "\u0D70\u0D71\u0D72\u0D73\u0D74\u0D75";

/* MYANMAR SHAN DIGIT ONE,
   SUNDANESE DIGIT TWO,
   LEPCHA DIGIT THREE,
   OL CHIKI DIGIT FOUR,
   VAI DIGIT FIVE,
   SAURASHTRA DIGIT SIX
   KAYAH LI DIGIT SEVEN
   CHAM DIGIT EIGHT
   JAVANESE DIGIT NINE 
   MEETEI MAYEK DIGIT ZERO */
const inDigits = "\u1091\u1BB2\u1C43\u1C54\uA625\uA8D6\uA907\uAA58\uA9D9\uABF0";
const inRomanNumerals = "\u2185\u2186\u2187\u2188";
const inSuperSubscripts = "\u2C7C\u2C7D\u2095\u209C";
    
const expectedTeluguFractions = "[0][1][2][3][1][2][3]";
const expectedMalayalamNumbers = "[10][100][1000][1/4][1/2][3/4]";
const expectedDigits = "1234567890";
const expectedRomanNumerals = "[6][50][50000][100000]";
const expectedSuperSubscripts = "v(j)^(V)v(h)v(t)";

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

    var outTeluguFractions = converter.Convert(inTeluguFractions);
    do_check_eq(outTeluguFractions, expectedTeluguFractions);

    var outMalayalamNumbers = converter.Convert(inMalayalamNumbers);
    do_check_eq(outMalayalamNumbers, expectedMalayalamNumbers);

    var outDigits = converter.Convert(inDigits);
    do_check_eq(outDigits, expectedDigits);

    var outRomanNumerals = converter.Convert(inRomanNumerals);
    do_check_eq(outRomanNumerals, expectedRomanNumerals);

    var outSuperSubscripts = converter.Convert(inSuperSubscripts);
    do_check_eq(outSuperSubscripts, expectedSuperSubscripts);
}
