/*
 * The with Statement
 */

function title() {
    return "The with Statement";
}

function run() {

    with(o) print('a') 
    with(o) print('a');
    with(o) { print('a') } 
}

/*
 * Copyright (c) 1999, Mountain View Compiler Company. All rights reserved.
 */
