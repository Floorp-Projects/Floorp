// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

class C {
    x = 5;
}

c = new C();

reportCompare(c.x, undefined); // TODO
//reportCompare(c.x, 5);

class D {
    #y = 5;
}

d = new D();

reportCompare(d.#y, undefined); // TODO
//reportCompare(d.#y, 5);
