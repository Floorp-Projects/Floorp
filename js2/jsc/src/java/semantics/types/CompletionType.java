package com.compilercompany.es3c.v1;

/**
 * class CompletionType
 */

public final class CompletionType extends ObjectType {
    public static final int normalType   = 0x00;
    public static final int breakType    = 0x01;
    public static final int continueType = 0x02;
    public static final int returnType   = 0x03;
    public static final int throwType    = 0x04;
	
	CompletionType() {
	    super("completion");
	}
}

/*
 * The end.
 */
