/*
 * The labeled Statement
 */

function title() {
    return "The labeled Statement";
}

function run() {
    a: print('a')
    b:
    print('b')
    c: {print('c')}
    d:;
}

/*
 * Copyright (c) 1999, Mountain View Compiler Company. All rights reserved.
 */
