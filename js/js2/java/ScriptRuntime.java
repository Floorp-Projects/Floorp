class ScriptRuntime {
    /*
     * There's such a huge space (and some time) waste for the Foo.class
     * syntax: the compiler sticks in a test of a static field in the
     * enclosing class for null and the code for creating the class value.
     * It has to do this since the reference has to get pushed off til
     * executiontime (i.e. can't force an early load), but for the
     * 'standard' classes - especially those in java.lang, we can trust
     * that they won't cause problems by being loaded early.
     */

    public final static Class StringClass = String.class;
    public final static Class NumberClass = Number.class;
    public final static Class BooleanClass = Boolean.class;
    public final static Class ByteClass = Byte.class;
    public final static Class ShortClass = Short.class;
    public final static Class IntegerClass = Integer.class;
    public final static Class LongClass = Long.class;
    public final static Class FloatClass = Float.class;
    public final static Class DoubleClass = Double.class;
    public final static Class CharacterClass = Character.class;
    public final static Class ObjectClass = Object.class;

    // This definition of NaN is identical to that in java.lang.Double
    // except that it is not final. This is a workaround for a bug in
    // the Microsoft VM, versions 2.01 and 3.0P1, that causes some uses
    // (returns at least) of Double.NaN to be converted to 1.0.
    // So we use ScriptRuntime.NaN instead of Double.NaN.
    public static double NaN = 0.0d / 0.0;

    /*
     * Helper function for toNumber, parseInt, and TokenStream.getToken.
     */
    static double stringToNumber(String s, int start, int radix) {
        char digitMax = '9';
        char lowerCaseBound = 'a';
        char upperCaseBound = 'A';
        int len = s.length();
        if (radix < 10) {
            digitMax = (char) ('0' + radix - 1);
        }
        if (radix > 10) {
            lowerCaseBound = (char) ('a' + radix - 10);
            upperCaseBound = (char) ('A' + radix - 10);
        }
        int end;
        double sum = 0.0;
        for (end=start; end < len; end++) {
            char c = s.charAt(end);
            int newDigit;
            if ('0' <= c && c <= digitMax)
                newDigit = c - '0';
            else if ('a' <= c && c < lowerCaseBound)
                newDigit = c - 'a' + 10;
            else if ('A' <= c && c < upperCaseBound)
                newDigit = c - 'A' + 10;
            else
                break;
            sum = sum*radix + newDigit;
        }
        if (start == end) {
            return NaN;
        }
        if (sum >= 9007199254740992.0) {
            if (radix == 10) {
                /* If we're accumulating a decimal number and the number 
                 * is >= 2^53, then the result from the repeated multiply-add 
                 * above may be inaccurate.  Call Java to get the correct 
                 * answer.
                 */
                try {
                    return Double.valueOf(s.substring(start, end)).doubleValue();
                } catch (NumberFormatException nfe) {
                    return NaN;
                }
            } else if (radix == 2 || radix == 4 || radix == 8 || 
                       radix == 16 || radix == 32) 
            {
                /* The number may also be inaccurate for one of these bases.
                 * This happens if the addition in value*radix + digit causes 
                 * a round-down to an even least significant mantissa bit 
                 * when the first dropped bit is a one.  If any of the 
                 * following digits in the number (which haven't been added 
                 * in yet) are nonzero then the correct action would have  
                 * been to round up instead of down.  An example of this 
                 * occurs when reading the number 0x1000000000000081, which 
                 * rounds to 0x1000000000000000 instead of 0x1000000000000100.
                 */
                BinaryDigitReader bdr = new BinaryDigitReader(radix, s, start, end);
                int bit;
                sum = 0.0;
    
                /* Skip leading zeros. */
                do {
                    bit = bdr.getNextBinaryDigit();
                } while (bit == 0);
    
                if (bit == 1) {
                    /* Gather the 53 significant bits (including the leading 1) */
                    sum = 1.0;
                    for (int j = 52; j != 0; j--) {
                        bit = bdr.getNextBinaryDigit();
                        if (bit < 0)
                            return sum;
                        sum = sum*2 + bit;
                    }
                    /* bit54 is the 54th bit (the first dropped from the mantissa) */
                    int bit54 = bdr.getNextBinaryDigit();
                    if (bit54 >= 0) {
                        double factor = 2.0;
                        int sticky = 0;  /* sticky is 1 if any bit beyond the 54th is 1 */
                        int bit3;
    
                        while ((bit3 = bdr.getNextBinaryDigit()) >= 0) {
                            sticky |= bit3;
                            factor *= 2;
                        }
                        sum += bit54 & (bit | sticky);
                        sum *= factor;
                    }
                }
            }
            /* We don't worry about inaccurate numbers for any other base. */
        }
        return sum;
    }

}