/*
 * The try Statement
 */

function title() {
    return "The try Statement";
}

function run() {
    try {
        print('a');
    } catch ( e:Exception ) {
        print('b');
    } catch ( e ) {
        print('b');
    } finally {
        print('c');
    }

}

/*
 * Copyright (c) 1999, Mountain View Compiler Company. All rights reserved.
 */
