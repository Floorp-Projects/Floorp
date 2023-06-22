function f(o) {
    return Object.keys(o)
}

po = {
    failure: 'hello'
}

o = {about: 5,
    ballisitic: 6,
    cakebread: 8,
    dalespeople: 9,
    evilproof: 20,
    fairgoing: 30,
    gargoylish: 2,
    harmonici: 1,
    jinniwink: 12,
    kaleidoscopical: 2,
    labellum: 1,
    macadamization: 4,
    neutrino: 1,
    observership: 0,
    quadratomandibular: 9,
    rachicentesis: 1,
    saltcat: 0,
    trousseau: 1,
    view: 10,
    wheelbox: 2,
    xerography: 1,
    yez: 3,
}
Object.setPrototypeOf(o, po);

// Initialize for-in cache for o.
for (var prop in o) {
    print(prop)
}

function test(o) {
    for (var i = 0; i<10; i++) {
        res = f(o);
        assertEq(false, res.includes("failure") );
        // assertEq(true, res.includes("1") );
    }
}

// Verify things.
test(o)


po[2] = "hi";
test(o);

po[3] = "bye";
for (var prop in o) {
    assertEq(prop == "gnome", false);
}
test(o);


o2 = {about: 5,
    ballisitic: 6,
    cakebread: 8,
    dalespeople: 9,
    evilproof: 20,
    fairgoing: 30,
    gargoylish: 2,
    harmonici: 1,
    jinniwink: 12,
    kaleidoscopical: 2,
    labellum: 1,
    macadamization: 4,
    neutrino: 1,
    observership: 0,
    quadratomandibular: 9,
    rachicentesis: 1,
    saltcat: 0,
    trousseau: 1,
    view: 10,
    wheelbox: 2,
    xerography: 1,
    yez: 3,
    "1": 10,
}

// Initialize for-in cache for o.
for (var prop in o2) {
    if (prop == "abra") print(prop);
}

// Verify things.
test(o2)


for (var i = 0; i < 20; i++) {
    assertEq(Object.keys(o2).includes("1"), true);
}

let o3 = {about: 5,
    ballisitic: 6,
    cakebread: 8,
    dalespeople: 9,
    evilproof: 20,
    fairgoing: 30,
    gargoylish: 2,
    harmonici: 1,
    jinniwink: 12,
    kaleidoscopical: 2,
    labellum: 1,
    macadamization: 4,
    neutrino: 1,
    observership: 0,
    quadratomandibular: 9,
    rachicentesis: 1,
    saltcat: 0,
    trousseau: 1,
    view: 10,
    wheelbox: 2,
    xerography: 1,
    yez: 3,
}

// Initialize for-in cache for o.
for (var prop in o2) {
    if (prop == "abra") print(prop);
}

// Verify things.
test(o3)


for (var i = 0; i < 20; i++) {
    assertEq(Object.keys(o3).includes("yez"), true);
}
