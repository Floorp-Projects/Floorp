load(libdir + "asserts.js");

const g = newGlobal({sameCompartmentAs: this});

let resolve, reject;
let promise = new Promise((resolveFn, rejectFn) => {
    resolve = resolveFn;
    reject = rejectFn;
});

// Set to a built-in Promise.prototype.then function, but from a different realm.
promise.then = g.Promise.prototype.then;

// Make SpeciesConstructor throw a TypeError exception.
promise.constructor = {
    [Symbol.species]: "not a constructor"
};

async function f(p) {
    await p;
}

let error;
f(promise).catch(e => { error = e; });

resolve(promise);

drainJobQueue();

assertEq(error.constructor === g.TypeError, true);
