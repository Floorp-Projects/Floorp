package com.compilercompany.es3c.v1;

/**
 * The interfaces for all types.
 */

public interface Type extends Init {
    Object[] values();
    Type[]   converts();
    void     addSub(Type type);
    boolean  includes(Value value);
    void     setSuper(Type type);
    Type     getSuper();
    Value    convert(Context context, Value value) throws Exception;
    Value    coerce(Context context, Value value) throws Exception;
}

/*
 * The end.
 */
