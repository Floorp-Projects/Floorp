class StackValue {
    
    StackValue(double x)
    {
        d = x;
        id = "";
    }

    StackValue(String x)
    {
        id = x;
        d = 0;
    }

    public String toString()
    {
        return "StackValue, id = " + id + ", d = " + d;
    }

    double d;
    String id;
    
}