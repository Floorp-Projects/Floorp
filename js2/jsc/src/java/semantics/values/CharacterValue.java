package com.compilercompany.es3c.v1;

/**
 * A character value.
 */

public class CharacterValue extends ObjectValue {

    private static final boolean debug = false;

    public String toString() {
        return "character("+value+")";
    }

    char value;

    CharacterValue(char value) {
        this.type  = CharacterType.type;
        this.value = value;
    }

    CharacterValue(char value, Value type) {
        this.type  = type;
        this.value = value;
    }

    public Value getValue(Context context) throws Exception { 

        Value value;

        if(type == CharacterType.type) {
            value = this;
        } else {
            throw new Exception("Unexpected type of character value.");
        }
    
        return value; 
    }
    
}

/*
 * The end.
 */
