class StackValue {
    
    StackValue(double v)
    {
        d = v;
    }
    
    StackValue(int v)
    {
        i = v;
    }
    
    StackValue(String v)
    {
        s = v;
    }
    
    StackValue(PostorderNodeIterator v)
    {
        p = v;
    }
    
    double d;
    PostorderNodeIterator p;
    String s;
    int i;


}