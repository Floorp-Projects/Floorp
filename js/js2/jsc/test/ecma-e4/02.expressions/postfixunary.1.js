/*
 * Postfix Expressions
 */

function title() {
    return "Postfix Expressions";
}

class T {}
class V {}

function run() {
    a.b::c.d[e](f,g)[h].i;
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@(T));
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class);

    a.b::c.d[e](f,g)[h].i;
    //a.b::c.d[e](f,g)[h].i.j::k;
    a.b::c.d[e](f,g)[h].i[j,k];
    a.b::c.d[e](f,g)[h].i(j,k);

    (function f(a,b){return a+b;});
    //(a<<b)::c;
    //new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z");
    //new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z").e::f;
    //new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z")[e,f];
    //new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z")(e,f);
    (const new const (a,b,c,1+2+3) "miles".(a<<b)::c[1,2].class++);
    (const new const function f(a,b){return a+b;}.class)::c[1,2].class--;
    (const new const function f(a,b){return a+b;}.class)::c[1,2].class@T;
    (const new const function f(a,b){return a+b;}.class)::c[1,2].class@(T);
}

/*
 * Copyright (c) 2000, Mountain View Compiler Company. 
 * All rights reserved.
 */
