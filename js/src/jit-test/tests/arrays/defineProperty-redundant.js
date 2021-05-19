var array = [123];
Object.freeze(array);
Object.defineProperty(array, 0, {});
Object.defineProperty(array, 0, {abc: 1});
Object.defineProperty(array, 0, {value: 123});
Object.defineProperty(array, 0, {writable: false});
Object.defineProperty(array, 0, {enumerable: true});
Object.defineProperty(array, 0, {configurable: false});
Object.defineProperty(array, 0, {value: 123, writable: false,
                                 enumerable: true, configurable: false});
