/*
 * Postfix Expressions
 */

function title() {
    return "Postfix Expressions";
}

function run() {
    a.b::c.d[e](f,g)[h].i;
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@(T+V));
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class);

    a.b::c.d[e](f,g)[h].i;
    a.b::c.d[e](f,g)[h].i.j::k;
    a.b::c.d[e](f,g)[h].i[j,k];
    a.b::c.d[e](f,g)[h].i(j,k);

    (function f(a,b){return a+b;});
    (a<<b)::c;
    new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z");
    new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z").e::f;
    new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z")[e,f];
    new const function f(a,b){return a+b;}.(a<<b)::c(d,1,"z")(e,f);
    (const new const (a,b,c,1+2+3) "miles".(a<<b)::c[1,2].class++);
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class--);
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@T);
    (const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@(T+V));
}

/*
 * Copyright (c) 1999, Mountain View Compiler Company. All rights reserved.
 */
