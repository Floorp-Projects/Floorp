function makeArrayIterator(array) {
    let i = 0;
    return {
        [Symbol.iterator]() {
            return {
                next() {
                    if (i >= array.length) {
                        return { done: true };
                    } else {
                        return { value: array[i++] };
                    }
                }
            };
        }
    }
}

function makeArrayIteratorWithHasMethod(array) {
    let i = 0;
    return {
        has(item) {
            return array.includes(item);
        },
        [Symbol.iterator]() {
            return {
                next() {
                    if (i >= array.length) {
                        return { done: true };
                    } else {
                        return { value: array[i++] };
                    }
                }
            };
        }
    };
}

function assertSetContainsExactOrderedItems(actual, expected) {
    assertEq(actual.size, expected.length);
    let index = 0;
    for (const item of actual) {
        assertEq(item, expected[index]);
        index++;
    }
}
