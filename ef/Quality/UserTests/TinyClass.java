import java.io.PrintStream;

// import java.lang.Exception;
final class MyException extends Exception
{
    String tag;

    public MyException(String inString)
    {
        tag = inString;
    }

    public String toString()
    {
        return tag;
    }

    public int intValue()
    {
        return Integer.parseInt(tag);
    }
}


//----------------------------------------------------------
// For JCKaastore01003
class Subclss1_of_Object extends Object
{
        public int cc1;
}

class Subclss2_of_Object extends Subclss1_of_Object
{
        public int cc2;
}
//----------------------------------------------------------
// For JCKaastore01102

interface SomeInterface1 {
        int SEVENS = 777;

        void printFields(PrintStream out);
}

class SomeClass1 implements SomeInterface1 {
        int i;

        public void printFields(PrintStream out) {
                out.print("i = ");
                out.println(i);
        }

        SomeClass1(int i) {
                this.i = i;
        }
}

interface AnotherInterface1 {
        int THIRDS = 33;

        void printFields(PrintStream out);
}

class AnotherClass1 implements AnotherInterface1 {
        int i;

        public void printFields(PrintStream out) {
                out.print("i = ");
                out.println(i);
        }

        AnotherClass1(int i) {
                this.i = i;
        }
}

//----------------------------------------------------------
// For JCKaastore01401
interface SomeInterface2 {
        int SEVENS = 777;
}

interface Subinterface2 extends SomeInterface2 {
        int THIRDS = 33;
}

class SomeClass2 implements Subinterface2 {
        int i;

        SomeClass2(int i) {
                this.i = i;
        }
}

class ImmediateSubclass2 extends SomeClass2 implements SomeInterface2 {
        float f;

        ImmediateSubclass2(int i, float f) {
                super(i);
                this.f = f;
        }
}

final class FinalSubclass2 extends ImmediateSubclass2 {
        double d;

        FinalSubclass2(int i, float f, double d) {
                super(i, f);
                this.d = d;
        }
}
//----------------------------------------------------------

// Very Simple tests that must pass before many serious tests will pass
public class TinyClass
{
    /*
    public static long longAdd(long a, long b)
    {
       return a + b;
    }

    public static long longAdd4(long a, long b, long c, long d)
    {
       return a + b + c + d;
    }

    public static int mulTest(int a, int b, int c, int d, int e, int f, int g, int h)
    {
        int aa = a * b;
        int cc = c * d;
        int ee = e * f;
        int gg = g * h;
        return a + b + c + d + e + f + g + h + aa + cc + ee + gg;
    }
    */

//----------------------------------------------------------
// Switch Test
    public static int switchTest(int a)
    {
        int c = 0;
        switch(a)
        {
            case 2: c = 1; break;
            case 6: c = 13; break;
            case 1: c = 1; break;
            case 3: c = 3; break;
            case 7: c = 21; break;
            case 4: c = 5; break;
            case 5: c = 8; break;
            case 8: c = 34; break;
            default: c = 55;
        }
        return c;
    }

    public static boolean switchTestAll()
    {
        boolean result = (
            (switchTest(2)  == 1)  &&
            (switchTest(6)  == 13) &&
            (switchTest(1)  == 1)  &&
            (switchTest(3)  == 3)  &&
            (switchTest(7)  == 21) &&
            (switchTest(4)  == 5)  &&
            (switchTest(5)  == 8)  &&
            (switchTest(8)  == 34) &&
            (switchTest(55) == 55) );
        report(result, "switchTestAll");
        return result;
    }

    public static long switchLong(int a)
    {
        long result = 0;
        switch(a)
        {
            case 0xcafebabe:    result = 0x1234567812345678L;    break;
            case 0x01234567:    result = 0xdeaddeaddeaddeadL;    break;
            case 0x11111111:    result = 0xffffffffffffffffL;    break;
            default:            result = 0xdeadc0dedeadc0deL;
        }
        return result;
    }

    public static boolean switchLongHarness()
    {
        long t1 = switchLong(0xcafebabe);
        long t2 = switchLong(0x01234567);
        long t3 = switchLong(0x11111111);
        long t4 = switchLong(0xffffffff);

        boolean r1 = t1 == 0x1234567812345678L;   // find out real values
        boolean r2 = t2 == 0xdeaddeaddeaddeadL;
        boolean r3 = t3 == 0xffffffffffffffffL;
        boolean r4 = t4 == 0xdeadc0dedeadc0deL;

        report(r1, "switchLong1");
        report(r2, "switchLong2");
        report(r3, "switchLong3");
        report(r4, "switchLong4");

        return(r1 && r2 && r3 && r4);
    }

    public static boolean switchTestNeg()
    {
        boolean result = (
            (switchTest( 0) == 55) &&
            (switchTest(-1) == 55) &&
            (switchTest(0xcafebabe)  == 55));
        report(result, "switchTestNeg");
        return result;
    }

    public static void switchTestHarness()
    {
        switchTestAll();
        switchTestNeg();
        switchLongHarness();
    }

//----------------------------------------------------------
// Seive Test
    public static void Sieve(int[] an)
	{
        int k1 = 1;
        int i2 = 0;
        int j2 = 0;

        j2++;
        an[0] = 1;
        an[1] = 2;
        k1 = 2;

        int length = an.length;
        for (int i1 = 3; k1 < length; i1++)
        {
            boolean flag1;
            i2 = 1;
            for (flag1 = true; i2 < k1 && flag1; i2++)
                if (an[i2] > 0 && an[i2] <= i1 / 2 && i1 % an[i2] == 0)
                    flag1 = false;
            if (flag1)
            {
                k1++;
                an[k1 - 1] = i1;
            }
        }
  	}

    public static boolean sieveHarness()
    {
  	    int a[] = new int[20];
        Sieve(a);
        boolean result = false;

        if( a[ 0]==1  && a[ 1]==2  && a[ 2]==3  && a[ 3]==5  && a[ 4]==7  &&
            a[ 5]==11 && a[ 6]==13 && a[ 7]==17 && a[ 8]==19 && a[ 9]==23 &&
            a[10]==29 && a[11]==31 && a[12]==37 && a[13]==41 && a[14]==43 &&
            a[15]==47 && a[16]==53 && a[17]==59 && a[18]==61 && a[19]==67 )
        {
            result = true;
        }
        else
        {
            for(int i = 0; i < 20; i++)
                System.out.println(a[i]);
        }

        report(result, "sieveTest");
        return result;
    }

//----------------------------------------------------------
// Instance Test
    public static boolean isStringObject(Object p)
    {
        return (p instanceof String);
    }

    public static boolean instanceTestHarness()
    {
        int[] i = new int[5];
        String s = new String("test");

        boolean result = (isStringObject(s) && !isStringObject(i));
        report(result, "instanceTest");
        return result;
    }

//----------------------------------------------------------
// ArgumentOrdering Test
// sinple test to ensure that arguments are being passed in in the correct order
    public static boolean orderTest(int a, int b, int c, int d)
    {
        return(a < b && b < c && c < d);
    }

    public static boolean orderTestHarness()
    {
        boolean result = orderTest(12, 45, 78, 128);
        report(result, "orderTest");
        return result;
    }

//----------------------------------------------------------
// Aritmetic Tests

    public static int negTest(int a, int b)
    {
        int bb = -a;
        int cc = -b;
        int c = 0;
        switch(a)
        {
            case 0:
                c += bb;
            case 1:
                c += cc;
            case 2:
                c += a;
            default:
                c += b;
        }
        return c;
    }

    public static boolean negTestHarness()
    {
        int a = 1;
        int b = 20;
        int intResult = negTest(a, b);
        // bb = -1
        // cc = -20
        // c= 0 + -20
        // c+= 1
        // c+= 20
        // c should == 1
        boolean result = (intResult == 1);
        report(result, "negTestHarness");
        return result;
    }

//----------------------------------------------------------
// Long Multiply
    public static long longMult(int temp, long a, long b, int temp2)
    {
       return a * b;
    }

    public static void testMulHarness()
    {
	    long mulResult = longMult(0xaa, 0x0dadbabeL, 0x10000L, 0xbb);
	    report(mulResult == 0x00000dadbabe0000L, "Long Multiply");
    }

//----------------------------------------------------------
// Integer Subtraction
    public static int subTest(int a, int b)
    {
        return a - b;
    }

    public static void subTestHarness()
    {
        report( 10 == subTest(56, 46),   "subTest1");
        report(111 == subTest(222, 111), "subTest2");
        report(-10 == subTest(12, 22),   "subTest3");
        report(993 == subTest(1000, 7),  "subTest4");
    }

//----------------------------------------------------------
// Integer Comparisons
    public static int cmpTestSigned(int a, int b)
    {
        if(a < b)
            return -1;
        if(a == b)
            return 0;
        else
            return 1;
    }

    public static void cmpTestsIntegers()
    {
        report( 1 == cmpTestSigned(34, 23),     "cmpTestsIntegers1");
        report(-1 == cmpTestSigned(0x55, 0x77), "cmpTestsIntegers2");
        report(-1 == cmpTestSigned(-5, 7),      "cmpTestsIntegers3");
        report( 0 == cmpTestSigned(1234, 1234), "cmpTestsIntegers4");
    }

    public static int cmpTestArray(int[] a, int b, int c)
    {
        int result = -99;
        try
        {
            if(a[b] < a[c])
                result = -1;
            else if(a[b] == a[c])
                result = 0;
            else
                result = 1;
        }
        catch (ArrayIndexOutOfBoundsException ex)
        {
            System.out.println("ArrayIndexOutOfBoundsException caught in cmpTestArray");
        }
        return result;
    }

    public static void cmpTestArrayHarness()
    {
        int[] a = new int[4];
        try
        {
            a[0] = 12;
            a[1] = 44;
            a[2] = 33;
            a[3] = 33;
        }
        catch (ArrayIndexOutOfBoundsException ex)
        {
            System.out.println("ArrayIndexOutOfBoundsException caught in cmpTestArrayHarness pt 1");
        }

        try
        {
            report( 1 == cmpTestArray(a, 1, 0), "cmpTestArray1");
            report( 0 == cmpTestArray(a, 2, 3), "cmpTestArray2");
            report(-1 == cmpTestArray(a, 0, 3), "cmpTestArray3");
            report( 1 == cmpTestArray(a, 1, 3), "cmpTestArray4");
        }
        catch (ArrayIndexOutOfBoundsException ex)
        {
            System.out.println("ArrayIndexOutOfBoundsException caught in cmpTestArrayHarness pt 2");
        }
    }

    public static void cmpTestHarness()
    {
        cmpTestsIntegers();
        cmpTestArrayHarness();
    }

//----------------------------------------------------------
// Test Integer Divides
    public static int divTest(int a, int b)
    {
        return a/b;
    }

    public static int modTest(int a, int b)
    {
        return a%b;
    }

    public static boolean divTestHarness(boolean debug)
    {
        boolean t1 = (divTest(12, 3)  == 4);
        boolean t2 = (divTest(12, 12) == 1);
        boolean t3 = (divTest(12, -1) == -12);
        boolean t4 = (divTest(110, 10)  == 11);
        if(debug)
        {
            report(t1, "div 12/3");
            report(t2, "div 12/12");
            report(t3, "div 12/-1");
            report(t4, "div 110/10");
        }
        boolean result = (t1 && t2 && t3 && t4);
        report(result, "divTestHarness");
        return result;
    }

    public static boolean modTestHarness(boolean debug)
    {
        boolean t1 = (modTest(12, 5)  == 2);
        boolean t2 = (modTest(12, 12) == 0);
        boolean t3 = (modTest(12, 14) == 12);
        boolean t4 = (modTest(10, 3)  == 1);
        if(debug)
        {
            report(t1, "mod1");
            report(t2, "mod2");
            report(t3, "mod3");
            report(t4, "mod4");
        }
        boolean result = (t1 && t2 && t3 && t4);
        report(result, "modTestHarness");
        return result;
    }

    public static boolean divModTest(int a, int b, boolean debug)
    {
        int quotient = divTest(a, b);
        int remainder = modTest(a, b);
        int original = (b * quotient) + remainder;

        boolean result = (a == original);
        if(debug)
        {
            String s = "divModTest " + a + "/" + b;
            if(!result)
                s += " q=" + quotient + " r=" + remainder + " o=" + original;
            report(result, s);
        }
        return result;
    }

    public static int divModSumTestHarness()
    {
        boolean verbose = true;
        int count = 0;
        if(divModTest(123, 5, verbose))
            count++;
        if(divModTest(45, 12, verbose))
            count++;
        if(divModTest(13, 14, verbose))
            count++;
        if(divModTest(0, 3, verbose))
            count++;

        report(count == 4, "divModSumTestHarness");
        return count;
    }


    public static int divModTestHarness()
    {
        int count = 0;
        if(divTestHarness(true))
            count++;
        if(modTestHarness(true))
            count++;
        if(divModSumTestHarness() == 4)
            count++;

        report(count == 3, "divModTestHarness");
        return count;
    }

//----------------------------------------------------------
// Test Long Comparisons

    public static boolean cmpLongLessThan(long a, long b)           { return (a < b);  }
    public static boolean cmpLongEqual(long a, long b)              { return (a == b); }
    public static boolean cmpLongGreaterThan(long a, long b)        { return (a > b);  }
    public static boolean cmpLongGreaterThanEqual(long a, long b)   { return (a >= b); }
    public static boolean cmpLongLessThanEqual(long a, long b)      { return (a <= b); }
    public static boolean cmpLongNotEqual(long a, long b)           { return (a != b); }

    public static int cmpLongTestHarnessHelper(long a, long b, boolean debug)
    {
        if(debug)
        {
    	    System.out.println("\n_______________________________________\nComparing " + a + " and " + b);
	    }
	    boolean lt = cmpLongLessThan(a, b);
	    boolean eq = cmpLongEqual(a, b);
	    boolean gt = cmpLongGreaterThan(a, b);

	    boolean geq = cmpLongGreaterThanEqual(a, b);
	    boolean leq = cmpLongLessThanEqual(a, b);
	    boolean neq = cmpLongNotEqual(a, b);

        if(debug)
        {
    	    System.out.print(lt ? "lt:true " : "lt:false ");
    	    System.out.print(eq ? "eq:true " : "eq:false ");
    	    System.out.print(gt ? "gt:true " : "gt:false ");
    	    System.out.print(geq ? "geq:true " : "geq:false ");
    	    System.out.print(leq ? "leq:true " : "leq:false ");
    	    System.out.println(neq ? "neq:true " : "neq:false ");
        }

        int count = 0;
        int returnValue = -255;
        if(lt)
        {
    	    if(debug) System.out.print(" <, ");
	        returnValue = -1;
	        count++;
        }

        if(eq)
        {
	        if(debug) System.out.print(" ==, ");
	        returnValue = 0;
	        count++;
        }

        if(gt)
        {
    	    if(debug) System.out.print(" >, ");
	        returnValue = 1;
	        count++;
        }

        try
        {
            if(count != 1)
            {
                throw new MyException("cmpLongTest failed: mutual exclusion failure 1");
            }

            if(eq && neq) throw new MyException("cmpLongTest failed: mutual exclusion failure 2");
            if(lt && geq) throw new MyException("cmpLongTest failed: mutual exclusion failure 3");
    	    if(gt && leq) throw new MyException("cmpLongTest failed: mutual exclusion failure 4");
    	    if(geq && leq && neq) throw new MyException("cmpLongTest failed: mutual exclusion failure 5");
    	}
    	catch (MyException ex)
    	{
    	    System.out.println(ex.toString());
    	}

        return returnValue;
    }

    public static void cmpLongTestHarness()
    {
        report( 1 == cmpLongTestHarnessHelper(34, 23, false), "Long Comparison Test: 34 > 23");
        report( 1 == cmpLongTestHarnessHelper(34, 23, false), "Long Comparison Test: 34 > 23");
        report(-1 == cmpLongTestHarnessHelper(0x55, 0x77, false), "Long Comparison Test: 0x55 < 0x77");
        report( 0 == cmpLongTestHarnessHelper(98, 98, false), "Long Comparison Test: 98 == 98");
        report( 1 == cmpLongTestHarnessHelper(1234, 6, false), "Long Comparison Test: 1234 > 6");

        report(-1 == cmpLongTestHarnessHelper(0x1234567812345678L, 0x2345678923456789L, false), "Long Comparison Test: a < b");
        report( 0 == cmpLongTestHarnessHelper(0x1234567812345678L, 0x1234567812345678L, false), "Long Comparison Test: a == b");
        report( 1 ==  cmpLongTestHarnessHelper(0x2345678923456789L, 0x1234567812345678L, false), "Long Comparison Test: a > b ");
    }

    public static void cmpLongSimple()
    {
        report(cmpLongLessThan(500, 1000), "cmpLongSimple");
    }

//----------------------------------------------------------
// Test Int Comparisons
    public static boolean cmpIntLessThan(int a, int b)
    {
        return (a < b);
    }

    public static void cmpIntSimple()
    {
        report(cmpIntLessThan(500, 1000), "cmpIntSimple");
    }

//----------------------------------------------------------
// Test Long Shifts
    public static long shiftLong(long a, int b)
    {
        return (a << b) + (a >> b) + (a >>> b);
    }

    public static void shlLongHarness()
    {
        long t1 = shiftLong(0x00000000cafebabeL, 0);
        long t2 = shiftLong(0xcafebabe00000000L, 16);
        long t3 = shiftLong(0x1234567812345678L, 32);
        long t4 = shiftLong(0x00000000deadbeefL, 64);

        boolean r1 = t1 == 0;   // find out real values
        boolean r2 = t2 == 0;
        boolean r3 = t3 == 0;
        boolean r4 = t4 == 0;

/*
        report(r1, "shiftLong1");
        report(r2, "shiftLong2");
        report(r3, "shiftLong3");
        report(r4, "shiftLong4");
        */
    }

    public static long convertToLongTest(int in)
    {
        return (long)in;
    }

    public static void convertToLongHarness()
    {
        boolean r1 = convertToLongTest(12) == 12L;
        boolean r2 = convertToLongTest(0xcafebabe) == 0xffffffffcafebabeL;
        boolean r3 = convertToLongTest(0xdeadbeef) == 0xffffffffdeadbeefL;
        boolean r4 = convertToLongTest(-1) == 0xffffffffffffffffL;

        report(r1, "convertToLong1");
        report(r2, "convertToLong2");
        report(r3, "convertToLong3");
        report(r4, "convertToLong4");
    }

    public static int convertToIntTest(long in, long in2)
    {
        return (int)in + (int)in2;
    }

    public static void convertToIntHarness()
    {
        int t1 = convertToIntTest(14L, 20L);
        int t2 = convertToIntTest(0x9999999966666666, 0x6666666699999999L);
        int t3 = convertToIntTest(-1L, -2L);
        int t4 = convertToIntTest(200L, 100L);

        boolean r1 = t1 == 34;
        boolean r2 = t2 == -1;
        boolean r3 = t3 == -3;
        boolean r4 = t4 == 300;

        report(r1, "convertToIntTest1");
        if(!r1) System.out.println("int1 = " + t1);

        report(r2, "convertToIntTest2");
        if(!r2) System.out.println("int2 = " + t2);

        report(r3, "convertToIntTest3");
        if(!r3) System.out.println("int3 = " + t3);

        report(r4, "convertToIntTest4");
        if(!r4) System.out.println("int3 = " + t4);
    }

    public static void convertHarness()
    {
        convertToLongHarness();
        convertToIntHarness();

        boolean r1 = convertToIntTest(convertToLongTest(5), convertToLongTest(10)) == 15;
        report(r1, "convertHarness");
    }

//----------------------------------------------------------
// Nested Exception Test
    private static void throwIf0(int a) throws MyException { if(a == 0) throw new MyException("0"); }
    private static void throwIf1(int a) throws MyException { if(a == 1) throw new MyException("1"); }
    private static void throwIf2(int a) throws MyException { if(a == 2) throw new MyException("2"); }
    private static void throwIf3(int a) throws MyException { if(a == 3) throw new MyException("3"); }
    private static void throwIf4(int a) throws MyException { if(a == 4) throw new MyException("4"); }
    private static void throwIf5(int a) throws MyException { if(a == 5) throw new MyException("5"); }
    private static void throwIf6(int a) throws MyException { if(a == 6) throw new MyException("6"); }
    private static void throwIf7(int a) throws MyException { if(a == 7) throw new MyException("7"); }

    public static void primeThrows()
    {
        try{
            throwIf0(-1); throwIf1(-1); throwIf2(-1); throwIf3(-1);
            throwIf4(-1); throwIf5(-1); throwIf6(-1); throwIf7(-1);
        }
        catch (MyException ex)
        {
          System.out.println("Serious error--primeThrows");
        }
    }

    // should return exactly 1 if 0 <= a <=7
    private static int nestedTryTest(int a)
    {
        int result = 0;
        try {
            try {

                throwIf6(a);

                try {

                    try {
                        throwIf3(a);
                        try { throwIf0(a); } catch (MyException ex) { result+= ex.intValue(); } // 0
                        try { throwIf1(a); } catch (MyException ex) { result+= ex.intValue(); } // 1
                    } catch (MyException ex) { result+= ex.intValue(); }                        // 3

                    throwIf4(a);

                    try {
                        throwIf5(a);
                        try { throwIf2(a); } catch (MyException ex) { result+= ex.intValue(); } // 2
                    } catch (MyException ex) { result+= ex.intValue(); }                        // 5

                 } catch (MyException ex) { result+= ex.intValue(); }                           // 4

              } catch (MyException ex) { result+= ex.intValue(); }                              // 6

            throwIf7(a);
         } catch (MyException ex) { result+= ex.intValue(); }                                   // 7

        return result;
    }

    public static void nestedTryTester()
    {
       primeThrows();

       if (false)
       {
            report(nestedTryTest(6) == 6, "nestedTry" + 6);
       }
       else
       {
            for(int i = 0; i < 8; i++)
            {
                int result = nestedTryTest(i);
                report(result == i, "nestedTry" + i);
                if(result != i)
                    System.out.println("result = " + result);
            }

            report(nestedTryTest(-1) == 0, "nestedTry" + -1);
            report(nestedTryTest(10) == 0, "nestedTry" + 10);
        }
    }

//----------------------------------------------------------
// Exception Test
// Simple Throw and Catch in same method
    public static int goo(int i)
        throws MyException
    {
        try
        {
            if(i == 23)
                throw new MyException("I am an exception!");
        }
        catch (ArithmeticException ex)
        {
            System.out.println("\n*** SHIT! I shouldn't be here!");
        }
        return i + 47;
    }

    public static int bar(int i)
        throws MyException
    {
        int p = i - 2 + goo(i);
        return p;
    }

    public static int foo(int i)
        throws MyException
    {
        int q = i + 37 - bar(i);
        return q;
    }

    public static boolean excTestSingleLocalThrowLocalCatch()
    {
        boolean result = false;
        try
        {
            throw new MyException("thrown exception");
        }
        catch (MyException ex)
        {
            result = true;
        }
        return result;
    }

    public static boolean excTestSingleRemoteThrowLocalCatch()
    {
        boolean result = false;
        try
        {
            foo(23);
        }
        catch (MyException ex)
        {
            result = true;
        }
        return result;
    }

    public static boolean excTestMultiRemoteThrowRemoteCatch()
    {
        int testCount = 0;
        for(int i = 0; i < 10; i++)
            if(excTestSingleLocalThrowLocalCatch())
                testCount++;
        return (testCount == 10);
    }

    // test multiple catches from a remote throw
    public static boolean excTestMultiRemoteThrowLocalCatch()
    {
        int testCount = 0;
        for(int i = 0; i < 10; i++)
        {
            try
            {
                foo(23);
            }
            catch (MyException ex)
            {
                testCount ++;
            }
        }
        return (testCount == 10);
    }

    // test multiple catches from a local throw
    public static boolean excTestMultiLocalThrowLocalCatch()
    {
        int testCount = 0;
        for(int i = 0; i < 10; i++)
        {
            try
            {
                throw new MyException("hi");
            }
            catch (MyException ex)
            {
                testCount ++;
            }
        }
        return (testCount == 10);
    }

    public static void excTestHarness()
    {
          report(excTestSingleLocalThrowLocalCatch(),  "excTestSingleLocalThrowLocalCatch");
          report(excTestSingleRemoteThrowLocalCatch(), "excTestSingleLocalThrowLocalCatch");
          report(excTestMultiRemoteThrowRemoteCatch(), "excTestMultiRemoteThrowRemoteCatch");
          report(excTestMultiLocalThrowLocalCatch(),   "excTestMultiLocalThrowLocalCatch");
          report(excTestMultiRemoteThrowLocalCatch(),  "excTestMultiRemoteThrowLocalCatch");
    }

//----------------------------------------------------------
    // call with a = 23, result = 1 + 2 + 4 + 8 + 16 + 32 == 63
    // otherwise result = 1 + 64 + 4 + 128 + 16 + 256 + 1024 == 1431
    private static int complexCatch(int a)
    {
        int result = 0;
        try
        {
            result = 1;
            try
            {
                foo(a);
                result += 64;
            }
            catch (MyException ex)
            {
                result += 2;
            }

            result += 4;
            try
            {
                foo(a);
                result += 128;
            }
            catch (MyException ex)
            {
                result += 8;
            }

            result += 16;
            foo(a);         // should generate exception here
            result += 256;

            try
            {
                foo(a);
            }
            catch (MyException ex)
            {
                result += 512;
            }
            result += 1024;
        }
        catch (MyException ex)
        {
            result += 32;
        }

        return result;      // should be 1 + 2 + 4 + 8
    }

    public static void complexCatchHarness()
    {
        int t0 = complexCatch(23);
        int t1 = complexCatch(22);

        boolean r0 = t0 == (1 + 2 + 4 + 8 + 16 + 32);
        boolean r1 = t1 == (1 + 64 + 4 + 128 + 16 + 256 + 1024);

        report(r0, "ComplexCatchTrue");
        if(!r0) System.out.println("ComplexCatchTrue = " + t0);

        report(r1, "ComplexCatchFalse");
        if(!r1) System.out.println("ComplexCatchFalse = " + t1);
    }

//==========================================================
// Store Long Test

    // store into an array and test that the values were actually stored
    public static void storeIntoLongArray(long[] a)
    {
        a[0] = 1;
        a[1] = 1;
        a[2] = 3;
        a[3] = 5;
        a[4] = 8;
        a[5] = 13;
        a[6] = 21;
        a[7] = 34;
    }

    public static boolean loadFromLongArray(long[] a)
    {
        if (    (a[0] !=  1) || (a[1] !=  1) || (a[2] !=  3) || (a[3] != 5) || (a[4] != 8) ||
                (a[5] != 13) || (a[6] != 21) || (a[7] != 34)    )
                return false;
        return true;
    }

    public static boolean longStoreTestHarness()
    {
        long[] a = new long[8];
        storeIntoLongArray(a);
        boolean r1 = loadFromLongArray(a);
        report(r1, "longStoreTest");
        return r1;
    }

    // no output!
    public static void storeLongTest2()
    {
        long[] l = new long[100];
        long b = 32;
        for(int i = 0; i < 100; i++)
        {
            b += i;
            l[i] = b;
        }
    }

    public static boolean simpleLongStoreTestHarness()
    {
        long[] a = new long[1];
        a[0] = 0xdeadbeefcafebabeL;
        boolean r1 = a[0] == 0xdeadbeefcafebabeL;
        report(r1, "simpleLongStoreTest");
        return r1;
    }

    public static boolean sillyLongConstTest()
    {
       return cmpLongEqual(0xdeadbeefcafebabeL, 0xdeadbeefcafebabeL);
    }

    // Test Addition
    public static long longAddTest(long a, long b)
    {
        return (a + b);
    }

    public static void longAddTestHarness()
    {
        long t0 = longAddTest(1000L, 1L);
        long t1 = longAddTest(0xdeadbeef00000000L, 0x00000000cafebabeL);
        long t2 = longAddTest(0x1010101010101010L, 0x0101010101010101L);
        long t3 = longAddTest(0x9999999999999999L, 0x6666666666666666L);

        boolean r0 = t0 == 1001;
        boolean r1 = t1 == 0xdeadbeefcafebabeL;
        boolean r2 = t2 == 0x1111111111111111L;
        boolean r3 = t3 == 0xffffffffffffffffL;

        report(r0, "longAddTest0");
        if(!r0) System.out.println("long0 = " + t0);

        report(r1, "longAddTest1");
        if(!r1) System.out.println("long1 = " + t1);

        report(r2, "longAddTest2");
        if(!r2) System.out.println("long2 = " + t2);

        report(r3, "longAddTest3");
        if(!r3) System.out.println("long3 = " + t3);
    }

    // Test 'Or' Operator
    public static long longOrTest(long a, long b)
    {
        return (a | b);
    }

    public static void longOrTestHarness()
    {
        long t0 = longOrTest(1000L, 1L);
        long t1 = longOrTest(0xdeadbeef00000000L, 0x00000000cafebabeL);
        long t2 = longOrTest(0x1010101010101010L, 0x0101010101010101L);
        long t3 = longOrTest(0x9999999999999999L, 0x6666666666666666L);

        boolean r0 = t0 == 1001;
        boolean r1 = t1 == 0xdeadbeefcafebabeL;
        boolean r2 = t2 == 0x1111111111111111L;
        boolean r3 = t3 == 0xffffffffffffffffL;

        report(r0, "longOrTest0");
        if(!r0) System.out.println("long0 = " + t0);

        report(r1, "longOrTest1");
        if(!r1) System.out.println("long1 = " + t1);

        report(r2, "longOrTest2");
        if(!r2) System.out.println("long2 = " + t2);

        report(r3, "longOrTest3");
        if(!r3) System.out.println("long3 = " + t3);
    }

    // tests lots of things
    public static String longToHexString(long a)
    {
        String result = "";

        for(int i = 0; i < 16; i++)
        {
            int digit = (int) (a & 0x000000000000000fL);
            switch(digit)
            {
                case 10: result = "a" + result; break;
                case 11: result = "b" + result; break;
                case 12: result = "c" + result; break;
                case 13: result = "d" + result; break;
                case 14: result = "e" + result; break;
                case 15: result = "f" + result; break;
                default: result = digit + result;
            }
            a = a >> 4;
        }

        return "0x" + result;
    }

    public static void longToHexStringTest()
    {
        String t1 = longToHexString (0xdeadbeefcafebabeL);
        boolean r1 = t1.equals("0xdeadbeefcafebabe");

        if(!r1)
            System.out.println("0xdeadbeefcafebabe != " + t1);

        report(r1, "longToHexStringTest");
    }

    public static void longTestHarness()
    {
        longAddTestHarness();
        longOrTestHarness();
        longStoreTestHarness();
        longToHexStringTest();
        simpleLongStoreTestHarness();
        report(sillyLongConstTest(), "sillyLongConstTest");
        storeLongTest2();   // no output
    }

//==========================================================
// Array Access Exceptions
    public static void testArrayStore(int[] b, int a)
    {
        b[a] = 3;
    }

    public static int arrayExceptionHelper(int[] array, int index)
    {
        int result = 0;
        try
        {
            testArrayStore(array, index);
        }
        catch (ArrayIndexOutOfBoundsException ex)
        {
            result = 0;
        }
        catch (IndexOutOfBoundsException ex)
        {
            result = 1;
        }
        catch (NullPointerException ex)
        {
            result = 2;
        }
        catch (NegativeArraySizeException ex)
        {
            result = 3;
        }
        return result;
    }

    public static boolean testNullPointerException()
    {
        boolean result = (arrayExceptionHelper(null, 1) == 2);
        report(result, "testNullPointerException");
        return result;
    }

    public static boolean testIndexOutOfBoundsException1()
    {
        boolean result = (arrayExceptionHelper(new int[10], 10) == 0);
        report(result, "testIndexOutOfBoundsException1");
        return result;
    }

    public static boolean testIndexOutOfBoundsException2()
    {
        boolean result = (arrayExceptionHelper(new int[10], -1) == 0);
        report(result, "testIndexOutOfBoundsException2");
        return result;
    }

    public static int[] makeNewArray(int a)
    {
        return new int[a];
    }

    public static boolean testNegativeArraySizeException()
    {
        boolean result = false;
        try
        {
            int[] a = makeNewArray(-5);
        }
        catch (NegativeArraySizeException ex)
        {
            result = true;
        }
        report(result, "testNegativeArraySizeException");
        return result;
    }

    public static void testArrayAccessExceptions()
    {
        int count = 0;
        if(testNullPointerException())
            count++;
        if(testNegativeArraySizeException())
            count++;
        if(testIndexOutOfBoundsException1())
            count++;
        if(testIndexOutOfBoundsException2())
            count++;
        report(count == 4, "testArrayAccessExceptions");
    }

//==========================================================
// Hardware Exceptions
    public static boolean testHardwareExceptions()
    {
        boolean result = false;
        try
        {
            divTest(3, 0);
        }
        catch (ArithmeticException ex)
        {
            result = true;
        }
        report(result, "testHardwareExceptions");
        return result;
    }


//==========================================================
// JCK Store Tests
      static  Object[] arrayref=new String[10];

      private static int JCKaastore00701b()
      {
            try
            {
               arrayref[1]=arrayref;
               return 2;
            } catch (ArrayStoreException e) {
                return 0;
            }
      }

      private static int JCKaastore00701c(PrintStream out)
      {
            try
            {
                arrayref[2]=out;
                return 3;
            }
            catch (ArrayStoreException e)
            {
            }
            return 0;
      }

      private static int JCKaastore00701(PrintStream out, boolean debug)
      {
//            if(debug) System.out.println("Begin JCKaastore00701");
            try {
                    arrayref[0]=null;
            } catch (ArrayStoreException e) {
                    return 1;
            }
 //           if(debug) System.out.println("done1 JCKaastore00701");
            try {
                    arrayref[1]=arrayref;
                    return 2;
            } catch (ArrayStoreException e) {
            }
 //           if(debug) System.out.println("done2 JCKaastore00701");
            try {
                    arrayref[2]=out;
                    return 3;
            } catch (ArrayStoreException e) {
            }
//            if(debug) System.out.println("done3 JCKaastore00701");
            return 0;
      }

    static final int len=10;
    static PrintStream outStream;
    private static int JCKaastore01003(PrintStream out)
    {
        Subclss1_of_Object arrayref[]=new Subclss1_of_Object[len];
        Subclss2_of_Object obj=new Subclss2_of_Object();

        outStream=out;
        try
        {
                obj.cc2=1;
                arrayref[1]=obj;
                outStream.println("aastore01003: Array Store Error");
                return 2;
        }
        catch (ArrayStoreException e)
        {
                return 0;
        }
    }

    private static int JCKaastore01102(PrintStream out)
    {
        SomeInterface1 u[]=new SomeInterface1[len];
        AnotherInterface1 v[]=new AnotherInterface1[len];;
        SomeClass1 x;
        AnotherClass1 y;

        outStream=out;
        x = new SomeClass1(1);
        y = new AnotherClass1(1);
        try
        {
                u[1] = x;
                v[1] = y;
                outStream.println("aastore01102: Array Store Error");
                return 2;
        }
        catch (ArrayStoreException e)
        {
                return 0;
        }
    }

    private static int JCKaastore01301(PrintStream out)
    {
            byte b1[]=new byte[2], b2[][]=new byte[3][4];
            short s1[]=new short[2], s2[][]=new short[3][4];
            int i1[]=new int[2], i2[][]=new int[3][4];
            long l1[]=new long[2], l2[][]=new long[3][4];
            char c1[]=new char[2], c2[][]=new char[3][4];
            float f1[]=new float[2], f2[][]=new float[3][4];
            double d1[]=new double[2], d2[][]=new double[3][4];

            outStream=out;
            try {
                    b2[0] = b1;
                    s2[0] = s1;
                    i2[0] = i1;
                    l2[0] = l1;
                    c2[0] = c1;
                    f2[0] = f1;
                    d2[0] = d1;
                    return 0;
            } catch (ArrayStoreException e) {
                    outStream.println("aastore01301: Array Store Error");
                    return 2;
            }
    }

    public static int JCKaastore01401a(PrintStream out)
    {
            SomeClass2 x [] [];
            try {
              x = new SomeClass2 [3] [];
              x[0] = new ImmediateSubclass2 [4];
                return 0;
            } catch (ArrayStoreException e) {
                return -1;
            }
    }

    public static int JCKaastore01401(PrintStream out)
    {
            int i [], j [];
            SomeInterface2 u [], v[] [];
            Subinterface2 w [];
            SomeClass2 x [] [];
            ImmediateSubclass2 y [];
            FinalSubclass2 z [];
            Object obj [];

            int result = 0;

            try {
                result = 1;
                i = new int [10];
                result = 2;
                i[0] = 777;
                result = 3;
                j = i;
                result = 4;
                x = new SomeClass2 [3] [];
                result = 5;
                x[0] = new ImmediateSubclass2 [4];
                result = 6;
                z = new FinalSubclass2 [4];
                result = 7;
                x[1] = z;
                result = 8;
                x[0][0] = new ImmediateSubclass2(1, 3.14f);
                result = 9;
                x[1][1] = new FinalSubclass2(-1, -3.14f, -2.71d);
                result = 10;
                u = x[0];
                result = 11;
                u = x[1];
                result = 12;
                v = x;
                result = 13;
                w = x[0];
                result = 14;
                u = w;
                result = 15;
                obj = x[1];
                result = 16;
                obj = x;
                result = 17;
                v = null;
                result = 18;
                v = (SomeInterface2 [] []) obj;
                return 0;
            } catch (ArrayStoreException e) {
                return result;
            }
    }

    public static boolean JCKStoreTests()
    {
        boolean result = true;

        result &= fullReport(JCKaastore00701b(), 0, "JCKaastore00701b");
        result &= fullReport(JCKaastore00701c(System.out), 0, "JCKaastore00701c");
        result &= fullReport(JCKaastore00701(System.out, true), 0, "JCKaastore00701");
        result &= fullReport(JCKaastore01003(System.out), 0, "JCKaastore01003");
        result &= fullReport(JCKaastore01102(System.out), 0, "JCKaastore01102");
        result &= fullReport(JCKaastore01401a(System.out), 0, "JCKaastore01401a");
        result &= fullReport(JCKaastore01401(System.out), 0, "JCKaastore01401");
        result &= fullReport(JCKaastore01301(System.out), 0, "JCKaastore01301");
        return result;
    }

//==========================================================
// JCK LongTests
   private static int JCKlsub00401()
    {
            long l1 = 0xFFFFFFFFFFFFFFFFl;
            long l2 = 0x7FFFFFFFFFFFFFFFl;
            long l3 = 0x8000000000000000l;
            long negOne = -1l;
            long res;

            if ((res = l1 - negOne) < 0)
                    return 1;
            else if (res != 0)
                    return 2;
            else if ((res = l2 - negOne) >= 0)
                    return 3;
            else if (res != l3)
                    return 4;
            else if ((res = l3 - 1l) <= 0)
                    return 5;
            else if (res != l2)
                    return 6;
            return 0;
    }

        public static boolean JCKLongTests()
        {
            boolean r0 = fullReport(JCKlsub00401(), 0, "JCKlsub00401");
            return r0;
        }

//==========================================================
// Byte/Float/Double stuff
    private static boolean simpleByteTest(byte value, boolean debug)
    {
        Byte b = new Byte(value);
        boolean result = true;

        result &= conditionalFullReport(b.floatValue() == (float)value,true, "simpleByteTest.float with " + value, debug);
        if(debug && (b.floatValue() == (float)value) == false)
            System.out.println("simpleByteTest.float: apparently " + b.floatValue() + " and " + (float)value + " aren't equal\n");

        result &= conditionalFullReport(b.doubleValue() == (double)value,true, "simpleByteTest.double with " + value, debug);
        if(debug && (b.doubleValue() == (double)value) == false)
            System.out.println("simpleByteTest.double: apparently " + b.doubleValue() + " and " + (double)value + " aren't equal\n");

        return result;
    }

    private static boolean byteTest(byte value, boolean debug)
    {
        Byte b = new Byte(value);
        boolean result = true;
        result &= conditionalFullReport(b.equals(new Byte(value)), true, "byteTest.equals with " + value, debug);
        result &= conditionalFullReport(b.hashCode() == value, true, "byteTest.hash with " + value, debug);
        result &= conditionalFullReport(b.intValue() == value,true, "byteTest.int with " + value, debug);
        result &= conditionalFullReport(b.longValue() == (long)value,true, "byteTest.long with " + value, debug);
        result &= conditionalFullReport(b.floatValue() == (float)value,true, "byteTest.float with " + value, debug);
        if(debug && (b.floatValue() == (float)value) == false)
            System.out.println("byteTest.float: apparently " + b.floatValue() + " and " + (float)value + " were supposed to be the same");

        result &= conditionalFullReport(b.doubleValue() == (double)value,true, "byteTest.double with " + value, debug);
        if(debug && (b.doubleValue() == (double)value) == false)
            System.out.println("byteTest.double: apparently " + b.doubleValue() + " and " + (double)value + " were supposed to be the same");

        result &= conditionalFullReport(b.shortValue() == (short)value,true, "byteTest.short with " + value, debug);
        return result;
    }

    private static boolean byteTestHelper(int value)
    {
        boolean result = true;

        boolean r1 = byteTest((byte)value, false);
        if(!r1)
            byteTest((byte)value, true);
        result &= r1;

        boolean r2 = simpleByteTest((byte)value, false);
        if(!r2)
            simpleByteTest((byte)value, true);
        result &= r2;

        return result;
    }

    public static void byteTestHarness()
    {
        stupidByteTestHarness();
        byteTestHelper(9);
        simpleByteTest((byte)9, true);
        byteTestHelper(9);
        simpleByteTest((byte)9, true);
        byteTestHelper(12);
        byteTestHelper(0);
        byteTestHelper(-3);
        byteTestHelper(17);
     }

//==========================================================
// Stupid Test
    // returns 3 if both tests pass
    private static int stupidByteTest(byte value)
    {
        Byte b = new Byte(value);
        int result = 0;

        if(b.floatValue() == (float)value)
            result |= 1;

        if(b.doubleValue() == (double)value)
            result |= 2;

        return result;
    }

    public static void stupidByteTestHarness()
    {
        for(byte i = (byte)-10; i <= 10; i++)
        {
            int result = stupidByteTest(i);
            if(result != 3)
            {
                if((result & 1) == 0)
                    System.out.println("stupidByteTest " + i + " (float) failed");
                if((result & 2) == 0)
                    System.out.println("stupidByteTest " + i + " (double) failed");
            }
        }
    }

//==========================================================
// Integer Object

    public static int hextoint(String s)
    {
        int sum = 0;
        int length = s.length();
        for(int i = 0; i < length; i++)
        {
            int digit = 0;
            char c = s.charAt(i);
            switch(c)
            {
                case '0':   digit =  0;  break;
                case '1':   digit =  1;  break;
                case '2':   digit =  2;  break;
                case '3':   digit =  3;  break;
                case '4':   digit =  4;  break;
                case '5':   digit =  5;  break;
                case '6':   digit =  6;  break;
                case '7':   digit =  7;  break;
                case '8':   digit =  8;  break;
                case '9':   digit =  9;  break;
                case 'a':   digit = 10;  break;
                case 'b':   digit = 11;  break;
                case 'c':   digit = 12;  break;
                case 'd':   digit = 13;  break;
                case 'e':   digit = 14;  break;
                case 'f':   digit = 15;  break;
                default:
                    System.out.println("hextointtest internal error: '" + c + "'");
            }
            sum = sum * 16 + digit;
        }
        return sum;
    }

    public static boolean inttohextest(int input)
    {
         String string = Integer.toHexString(input);
         int    integer = hextoint(string);

         boolean result = (input == integer);

         if(!result)
         {
             System.out.print("in:" + input);
             System.out.print(", string:'" + string + "'");
             System.out.println(", out:" + integer);
         }

         return result;
    }

    public static void simplehextest()
    {
        report(Integer.toHexString(12).equals("c"), "simplehextest1");
        report(Integer.toHexString(-1).equals("ffffffff"), "simplehextest2");
        report(Integer.toHexString(0).equals("0"), "simplehextest3");
    }

    public static void betterHexTest()
    {
        report(inttohextest(27), "integerHexTest1");
        report(inttohextest(-1), "integerHexTest1");
        report(inttohextest(255), "integerHexTest1");
    }

    // From JDK -- Convert the integer to an unsigned number.
    private static String toUnsignedString(int i, int shift, boolean debug)
    {

        StringBuffer buf = new StringBuffer(shift >= 3 ? 11 : 32);
        int radix = 1 << shift;
        int mask = radix - 1;

        if(debug)
            System.out.println("radix: " + radix + "\nmask: " + mask);

        do
        {
            buf.append(Character.forDigit(i & mask, radix));

            if(debug)
                System.out.println(
                    "i=" + i + " i&mask=" + (i & mask) + " radix=" +
                    radix + " --> append: '" + Character.forDigit(i & mask, radix) + "'");

            i >>>= shift;
        } while (i != 0);

        if(debug)
            System.out.println("before reversing: '" + buf + "'\n");

        return buf.reverse().toString();
    }

    private static boolean mytoHexTest(int input, String expected)
    {
        String hex = toUnsignedString(input, 4, false);
        boolean result = hex.equals(expected);

        report(result, "mytoHexTest" + expected);
        if(!result)
        {
            System.out.println("in: " + input + "  out:'" + hex + "' expected:'" + expected + "'");
            toUnsignedString(input, 4, true);
        }

        return result;
    }

    public static boolean mytoHexTestHarness()
    {
        boolean r0 = mytoHexTest(0x00, "0");
        boolean r1 = mytoHexTest(0x0a, "a");
        boolean r2 = mytoHexTest(0xdeadbeef, "deadbeef");
        boolean r3 = mytoHexTest(-1, "ffffffff");
        return r0 && r1 && r2 && r3;
    }

    public static void integerHarness()
    {
        mytoHexTestHarness();
        simplehextest();
        betterHexTest();
    }

//==========================================================
// Simple Character Tests
    public static boolean charForDigitTestSimple()
    {
        boolean r0 =
            Character.forDigit(0x00, 16) == '0' &&
            Character.forDigit(0x01, 16) == '1' &&
            Character.forDigit(0x02, 16) == '2' &&
            Character.forDigit(0x03, 16) == '3' &&
            Character.forDigit(0x04, 16) == '4' &&
            Character.forDigit(0x05, 16) == '5' &&
            Character.forDigit(0x06, 16) == '6' &&
            Character.forDigit(0x07, 16) == '7' &&
            Character.forDigit(0x08, 16) == '8' &&
            Character.forDigit(0x09, 16) == '9' &&
            Character.forDigit(0x0a, 16) == 'a' &&
            Character.forDigit(0x0b, 16) == 'b' &&
            Character.forDigit(0x0c, 16) == 'c' &&
            Character.forDigit(0x0d, 16) == 'd' &&
            Character.forDigit(0x0e, 16) == 'e' &&
            Character.forDigit(0x0f, 16) == 'f';

        report(r0, "charForDigitTestSimple");
        return r0;
    }

    public static boolean charForDigitTest()
    {
        char t0 = Character.forDigit(0x0a, 16);
        boolean r0 = t0 == 'a';

        char t1 = Character.forDigit(0, 2);
        boolean r1 = t1 == '0';

        char t2 = Character.forDigit(15, 10);
        boolean r2 = t2 == '\u0000';

        char t3 = Character.forDigit(9, 36);
        boolean r3 = t3 == '9';

        report(r0, "charForDigitTest0");
        if(!r0) System.out.println("char0 = '" + t0 + "'");

        report(r1, "charForDigitTest1");
        if(!r1) System.out.println("char1 = '" + t1 + "'");

        report(r2, "charForDigitTest2");
        if(!r2) System.out.println("char2 = '" + t2 + "'");

        report(r3, "charForDigitTest3");
        if(!r3) System.out.println("char3 = '" + t3 + "'");

        return r0 && r1 && r2 && r3;
    }

    public static void characterTests()
    {
        charForDigitTestSimple();
        charForDigitTest();
    }

//==========================================================
// Utility

    public static boolean conditionalFullReport(String result, String expected, String testName, boolean debug)
    {
        boolean r0 = result.equals(expected);
        if (debug)
        {
            String text = testName;
            if(!r0)
                text += " returned " + result + "(expected " + expected + ")";
            report(r0, text, debug);
        }
        return r0;
    }

    public static boolean conditionalFullReport(boolean result, boolean expected, String testName, boolean debug)
    {
        boolean r0 = result == expected;
        if (debug)
        {
            String text = testName;
            if(!r0)
                text += " returned " + result;
            report(r0, text, debug);
        }
        return r0;
    }

    public static boolean fullReport(int result, int expected, String testName)
    {
        boolean r0 = result == expected;
        String text = testName;
        if(!r0)
            text += " returned " + result;
        report(r0, text);
        return r0;
    }

    public static void report(boolean result, String testName)
    {
        report(result, testName, true);
    }
    public static void report(boolean result, String testName, boolean debug)
    {
        //if(true)
        {
            if(result)
    	        System.out.print(" Passed  ");
            else
                System.out.print("*Failed* ");

            System.out.println("'" + testName + "'");
        }
        /*
        else
        {
            System.out.println("\n%\nNAME: " + testName);
            if(result)
    	        System.out.print("STATUS: PASSED.");
            else
                System.out.print("STATUS: FAILED.");
        }
        */
    }

//==========================================================
// Main

    public static void runAllTests()
    {
	    System.out.println("Begin Tests");
        subTestHarness();
        switchTestHarness();
        excTestHarness();
        complexCatchHarness();
        cmpLongTestHarness();
        cmpLongSimple();
        cmpIntSimple();
	    convertHarness();
	    byteTestHarness();
        longTestHarness();
        testMulHarness();
	    cmpTestHarness();
	    shlLongHarness();
	    testArrayAccessExceptions();
        orderTestHarness();
        instanceTestHarness();
        negTestHarness();
        divModTestHarness();
        sieveHarness();
        testHardwareExceptions();
        characterTests();
        integerHarness();
        JCKLongTests();
   //     JCKStoreTests();
	    nestedTryTester();
	    System.out.println("End Tests");
    }

    public static float fadd(float a, float b)
    {
        if (a < 3.0 && b > 12.5)
            return a + b;
        else
            return b - a;
    }

/*
    public static int idiv(int a, int b)
    {
        return a/b;
    }
*/
	public static void main(String[] args)
	{
        runAllTests();
      // fadd(12.5f, 34.5f);
    //  idiv(0x80000000, 0xffffffff);
    }
}