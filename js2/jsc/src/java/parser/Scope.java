package com.compilercompany.es3c.v1;

/**
 * A scope, which contains slots.
 */

public interface Scope {
    Slot get(Value namespace, String name) throws Exception;
    Slot add(Value namespace, String name ) throws Exception;
    boolean has(Value namespace, String name) throws Exception;
}

/*
 * The end.
 */
