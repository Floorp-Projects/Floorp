/*
 * Prefix Unary Expressions
 */

function title() {
    return "Prefix Unary Expressions";
}

function run() {
    aa:b::c.d[e](f,g)[h].i++;
    ba:delete p;
    ca:void 0
    da:typeof ob;
    ea:++a.b--;
    fa:--a.b++;
    ga:+a;
    ha:-a;
    ia:!a;
    ja:a;
    ka:a*b;
    la:a/b;
    ma:a%b;
    na:a*b;
    oa:a*b+c;
    pa:a*b-c;
    qa:a*b-c;
    ra:a*b-c<<2;
    sa:a*b-c>>2;
    ta:a*b-c>>>2;
    ua:a*b-c>>>2;
    va:a*b-c>>>2<0;
    wa:a*b-c>>>2>0;
    xa:a*b-c>>>2<=0;
    ya:a*b-c>>>2>=0;
    za:a*b-c>>>2 instanceof int;
    ab:a*b-c>>>2 in ob;
    bb:a*b-c>>>2;
    cb:a*b-c>>>2<0;
    db:a*b-c>>>2>0;
    eb:a*b-c>>>2<=0;
    fb:a*b-c>>>2>=0;
    gb:a*b-c>>>2 instanceof int;
    hb:a*b-c>>>2<0;
    ib:a*b-c>>>2<0==a*b-c>>>2<0;
    jb:a*b-c>>>2<0!=a*b-c>>>2<0;
    kb:a*b-c>>>2<0===a*b-c>>>2<0;
    lb:a*b-c>>>2<0!=a*b-c>>>2<0;
    mb:a*b-c>>>2<0==a*b-c>>>2<0;
    nb:a*b-c>>>2<0==a*b-c>>>2<0&0xff;
    ob:a*b-c>>>2<0==a*b-c>>>2<0&0xff;
    pb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10;
    qb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10;
    rb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f;
    sb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f;
    tb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f&&true;
    ub:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f&&true^^!true;
    vb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f&&true^^!true;
    wb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f&&true^^!true;
    xb:a*b-c>>>2<0==a*b-c>>>2<0&0xff^0x10|0x0f&&true^^!true||false;
}

/*
 * Copyright (c) 1999-2001, Mountain View Compiler Company. 
 * All rights reserved.
 */
