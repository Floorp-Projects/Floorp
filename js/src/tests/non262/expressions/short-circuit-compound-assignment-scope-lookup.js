// |reftest| skip-if(release_or_beta)

// Test scope lookups are executed in the correct order.

function createScope() {
  let log = [];
  let environment = {};
  let scope = new Proxy(environment, new Proxy({
    has(target, property) {
      log.push({target, property});
      return Reflect.has(target, property);
    },
    get(target, property, receiver) {
      log.push({target, property, receiver});
      return Reflect.get(target, property, receiver);
    },
    set(target, property, value, receiver) {
      log.push({target, property, value, receiver});
      return Reflect.set(target, property, value, receiver);
    },
    getOwnPropertyDescriptor(target, property) {
      log.push({target, property});
      return Reflect.getOwnPropertyDescriptor(target, property);
    },
    defineProperty(target, property, descriptor) {
      log.push({target, property, descriptor});
      return Reflect.defineProperty(target, property, descriptor);
    },
  }, {
    get(target, property, receiver) {
      log.push(property);
      return Reflect.get(target, property, receiver);
    }
  }));

  return {log, environment, scope};
}

// AndAssignExpr
{
  let {log, environment, scope} = createScope();

  environment.a = true;

  with (scope) {
    a &&= false;
  }
  assertEq(environment.a, false);

  with (scope) {
    a &&= true;
  }
  assertEq(environment.a, false);

  assertDeepEq(log, [
    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},

    // Reference Type, 6.2.4.9 PutValue ( V, W )
    // Object Environment Records, 8.1.1.2.5 SetMutableBinding ( N, V, S )
    "set", {target: environment, property: "a", value: false, receiver: scope},

    // Ordinary Objects, 9.1.9 [[Set]] ( P, V, Receiver )
    // Ordinary Objects, 9.1.9.1 OrdinarySet ( O, P, V, Receiver )
    // Ordinary Objects, 9.1.9.2 OrdinarySetWithOwnDescriptor ( O, P, V, Receiver, ownDesc )
    "getOwnPropertyDescriptor", {target: environment, property: "a"},
    "defineProperty", {target: environment, property: "a", descriptor: {value: false}},

    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},
  ]);
}

// OrAssignExpr
{
  let {log, environment, scope} = createScope();

  environment.a = false;

  with (scope) {
    a ||= true;
  }
  assertEq(environment.a, true);

  with (scope) {
    a ||= false;
  }
  assertEq(environment.a, true);

  assertDeepEq(log, [
    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},

    // Reference Type, 6.2.4.9 PutValue ( V, W )
    // Object Environment Records, 8.1.1.2.5 SetMutableBinding ( N, V, S )
    "set", {target: environment, property: "a", value: true, receiver: scope},

    // Ordinary Objects, 9.1.9 [[Set]] ( P, V, Receiver )
    // Ordinary Objects, 9.1.9.1 OrdinarySet ( O, P, V, Receiver )
    // Ordinary Objects, 9.1.9.2 OrdinarySetWithOwnDescriptor ( O, P, V, Receiver, ownDesc )
    "getOwnPropertyDescriptor", {target: environment, property: "a"},
    "defineProperty", {target: environment, property: "a", descriptor: {value: true}},

    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},
  ]);
}

// CoalesceAssignExpr
{
  let {log, environment, scope} = createScope();

  environment.a = null;

  with (scope) {
    a ??= true;
  }
  assertEq(environment.a, true);

  with (scope) {
    a ??= false;
  }
  assertEq(environment.a, true);

  assertDeepEq(log, [
    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},

    // Reference Type, 6.2.4.9 PutValue ( V, W )
    // Object Environment Records, 8.1.1.2.5 SetMutableBinding ( N, V, S )
    "set", {target: environment, property: "a", value: true, receiver: scope},

    // Ordinary Objects, 9.1.9 [[Set]] ( P, V, Receiver )
    // Ordinary Objects, 9.1.9.1 OrdinarySet ( O, P, V, Receiver )
    // Ordinary Objects, 9.1.9.2 OrdinarySetWithOwnDescriptor ( O, P, V, Receiver, ownDesc )
    "getOwnPropertyDescriptor", {target: environment, property: "a"},
    "defineProperty", {target: environment, property: "a", descriptor: {value: true}},

    // Execution Contexts, 8.3.2 ResolveBinding ( name [ , env ] )
    // Lexical Environments, 8.1.2.1 GetIdentifierReference ( lex, name, strict )
    // Object Environment Records, 8.1.1.2.1 HasBinding ( N )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: Symbol.unscopables, receiver: scope},

    // Reference Type, 6.2.4.8 GetValue ( V )
    // Object Environment Records, 8.1.1.2.6 GetBindingValue ( N, S )
    "has", {target: environment, property: "a"},
    "get", {target: environment, property: "a", receiver: scope},
  ]);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
