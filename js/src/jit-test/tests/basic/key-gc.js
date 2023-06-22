function f(o) {
    return Object.keys(o)
}

function test(o) {
    for (var i = 0; i<10; i++) {
        res = f(o);
        assertEq(true, res.includes("cakebread") );

        // Initialize for-in cache for o.
        for (var prop in o) {
            if (prop == "abra") print(prop);
        }

    }
}

let obj = {about: 5,
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

// Verify things.

// Collect after every allocation to shake loose issues
gczeal(2,1);
test(obj)
