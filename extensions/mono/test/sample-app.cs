using System;
using Mozilla.XPCOM;
using Interfaces = Mozilla.XPCOM.Interfaces;


public class Test
{
    class TestCallback : Mozilla.XPCOM.Interfaces.testCallback
    {
        public void Call() {
            Console.WriteLine("Callback invoked!");
        }
    }

    public static void Main()
    {
        Interfaces.test myTest = (Interfaces.test)
            Components.CreateInstance("@off.net/test-component;1",
                                      typeof(Interfaces.test));
        Console.WriteLine("3 + 5 = {0}", myTest.Add(3, 5));
        int before = myTest.IntProp;
        myTest.IntProp = 99;
        Console.WriteLine("intProp: {0}, (= 99), {1}", before, myTest.IntProp);
        Console.WriteLine("roIntProp: {0}", myTest.RoIntProp);
        Console.WriteLine("Invoking callback:");
        TestCallback tcb = new TestCallback();
        myTest.Callback(tcb);
        Console.WriteLine("Done!");
    }
}
