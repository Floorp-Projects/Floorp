package com.compilercompany.es3c.v1;

/**
 * Interface that object initializers implement.
 *
 * This interface is implemented by objects that can build
 * instances of the Value class. For example, there is an
 * object class that implements this interface for the
 * system global object.
 */

interface Init {
    void init( Value ob ) throws Exception;
}

/*
 * The end.
 */
