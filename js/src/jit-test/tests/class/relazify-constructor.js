class WithConstructor {
    constructor() {}
}

class DefaultConstructor {
}

class WithConstructorDerived extends DefaultConstructor {
    constructor() {
        super()
    }
}

class DefaultConstructorDerived extends DefaultConstructor {
}

relazifyFunctions();

let a = new WithConstructor;
let b = new DefaultConstructor;
let c = new WithConstructorDerived;
let d = new DefaultConstructorDerived;
