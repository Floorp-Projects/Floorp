if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.Iter'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.Iter'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_Iter = function (t) {
    t.is( sum([1, 2, 3, 4, 5]), 15, "sum works on Arrays" );
    t.is( compare(list([1, 2, 3]), [1, 2, 3]), 0, "list([x]) == [x]" );
    t.is( compare(list(range(6, 0, -1)), [6, 5, 4, 3, 2, 1]), 0, "list(range(6, 0, -1)");
    t.is( compare(list(range(6)), [0, 1, 2, 3, 4, 5]), 0, "list(range(6))" );
    var moreThanTwo = partial(operator.lt, 2);
    t.is( sum(ifilter(moreThanTwo, range(6))), 12, "sum(ifilter(, range()))" ); 
    t.is( sum(ifilterfalse(moreThanTwo, range(6))), 3, "sum(ifilterfalse(, range()))" ); 

    var c = count(10);
    t.is( compare([c.next(), c.next(), c.next()], [10, 11, 12]), 0, "count()" );
    c = cycle([1, 2]);
    t.is( compare([c.next(), c.next(), c.next()], [1, 2, 1]), 0, "cycle()" );
    c = repeat("foo", 3);
    t.is( compare(list(c), ["foo", "foo", "foo"]), 0, "repeat()" );
    c = izip([1, 2], [3, 4, 5], repeat("foo"));
    t.is( compare(list(c), [[1, 3, "foo"], [2, 4, "foo"]]), 0, "izip()" );

    t.is( compare(list(range(5)), [0, 1, 2, 3, 4]), 0, "range(x)" );
    c = islice(range(10), 0, 10, 2);
    t.is( compare(list(c), [0, 2, 4, 6, 8]), 0, "islice(x, y, z)" );

    c = imap(operator.add, [1, 2, 3], [2, 4, 6]);
    t.is( compare(list(c), [3, 6, 9]), 0, "imap(fn, p, q)" );

    c = filter(partial(operator.lt, 1), iter([1, 2, 3]));
    t.is( compare(c, [2, 3]), 0, "filter(fn, iterable)" );

    c = map(partial(operator.add, -1), iter([1, 2, 3]));
    t.is( compare(c, [0, 1, 2]), 0, "map(fn, iterable)" );

    c = map(operator.add, iter([1, 2, 3]), [2, 4, 6]);
    t.is( compare(c, [3, 6, 9]), 0, "map(fn, iterable, q)" );

    c = map(operator.add, iter([1, 2, 3]), iter([2, 4, 6]));
    t.is( compare(c, [3, 6, 9]), 0, "map(fn, iterable, iterable)" );

    c = applymap(operator.add, [[1, 2], [2, 4], [3, 6]]);
    t.is( compare(list(c), [3, 6, 9]), 0, "applymap()" );

    c = applymap(function (a) { return [this, a]; }, [[1], [2]], 1);
    t.is( compare(list(c), [[1, 1], [1, 2]]), 0, "applymap(self)" );

    c = chain(range(2), range(3));
    t.is( compare(list(c), [0, 1, 0, 1, 2]), 0, "chain(p, q)" );

    var lessThanFive = partial(operator.gt, 5);
    c = takewhile(lessThanFive, count());
    t.is( compare(list(c), [0, 1, 2, 3, 4]), 0, "takewhile()" );

    c = dropwhile(lessThanFive, range(10));
    t.is( compare(list(c), [5, 6, 7, 8, 9]), 0, "dropwhile()" );

    c = tee(range(5), 3);
    t.is( compare(list(c[0]), list(c[1])), 0, "tee(..., 3) p0 == p1" );
    t.is( compare(list(c[2]), [0, 1, 2, 3, 4]), 0, "tee(..., 3) p2 == fixed" );

    t.is( compare(reduce(operator.add, range(10)), 45), 0, "reduce(op.add)" );

    try {
        reduce(operator.add, []);
        t.ok( false, "reduce didn't raise anything with empty list and no start?!" );
    } catch (e) {
        if (e instanceof TypeError) {
            t.ok( true, "reduce raised TypeError correctly" );
        } else {
            t.ok( false, "reduce raised the wrong exception?" );
        }
    }

    t.is( reduce(operator.add, [], 10), 10, "range initial value OK empty" );
    t.is( reduce(operator.add, [1], 10), 11, "range initial value OK populated" );

    t.is( compare(iextend([1], range(2)), [1, 0, 1]), 0, "iextend(...)" );

    var x = [];
    exhaust(imap(bind(x.push, x), range(5)));
    t.is( compare(x, [0, 1, 2, 3, 4]), 0, "exhaust(...)" );

    t.is( every([1, 2, 3, 4, 5, 4], lessThanFive), false, "every false" );
    t.is( every([1, 2, 3, 4, 4], lessThanFive), true, "every true" );
    t.is( some([1, 2, 3, 4, 4], lessThanFive), true, "some true" );
    t.is( some([5, 6, 7, 8, 9], lessThanFive), false, "some false" );
    t.is( some([5, 6, 7, 8, 4], lessThanFive), true, "some true" );

    var rval = [];
    forEach(range(2), rval.push, rval);
    t.is( compare(rval, [0, 1]), 0, "forEach works bound" );

    function foo(o) {
        rval.push(o);
    }
    forEach(range(2), foo);
    t.is( compare(rval, [0, 1, 0, 1]), 0, "forEach works unbound" );
    
    t.is( compare(sorted([3, 2, 1]), [1, 2, 3]), 0, "sorted default" );
    rval = sorted(["aaa", "bb", "c"], keyComparator("length"));
    t.is(compare(rval, ["c", "bb", "aaa"]), 0, "sorted custom");

    t.is( compare(reversed(range(4)), [3, 2, 1, 0]), 0, "reversed iterator" );
    t.is( compare(reversed([5, 6, 7]), [7, 6, 5]), 0, "reversed list" );

    var o = {lst: [1, 2, 3], iterateNext: function () { return this.lst.shift(); }};
    t.is( compare(list(o), [1, 2, 3]), 0, "iterateNext" );


    function except(exc, func) {
        try {
            func();
            t.ok(false, exc.name + " was not raised.");
        } catch (e) {
            if (e == exc) {
                t.ok( true, "raised " + exc.name + " correctly" );
            } else {
                t.ok( false, "raised the wrong exception?" );
            }
        }
    }

    odd = partial(operator.and, 1)

    // empty
    grouped = groupby([]);
    except(StopIteration, grouped.next);

    // exhaust sub-iterator
    grouped = groupby([2,4,6,7], odd);
    kv = grouped.next(); k = kv[0], subiter = kv[1];
    t.is(k, 0, "odd(2) = odd(4) = odd(6) == 0");
    t.is(subiter.next(), 2, "sub-iterator.next() == 2");
    t.is(subiter.next(), 4, "sub-iterator.next() == 4");
    t.is(subiter.next(), 6, "sub-iterator.next() == 6");
    except(StopIteration, subiter.next);
    kv = grouped.next(); key = kv[0], subiter = kv[1];
    t.is(key, 1, "odd(7) == 1");
    t.is(subiter.next(), 7, "sub-iterator.next() == 7");
    except(StopIteration, subiter.next);

    // not consume sub-iterator
    grouped = groupby([2,4,6,7], odd);
    kv = grouped.next(); key = kv[0], subiter = kv[1];
    t.is(key, 0, "0 = odd(2) = odd(4) = odd(6)");
    kv = grouped.next(); key = kv[0], subiter = kv[1];
    t.is(key, 1, "1 = odd(7)");
    except(StopIteration, grouped.next);

    // consume sub-iterator partially
    grouped = groupby([3,1,1,2], odd);
    kv = grouped.next(); key = kv[0], subiter = kv[1];
    t.is(key, 1, "odd(1) == 1");
    t.is(subiter.next(), 3, "sub-iterator.next() == 3");
    kv = grouped.next(); key = kv[0], v = kv[1];
    t.is(key, 0, "skip (1,1),  odd(2) == 0");
    except(StopIteration, grouped.next);

    // null
    grouped = groupby([null,null]);
    kv = grouped.next(); k = kv[0], v = kv[1];
    t.is(k, null, "null ok");

    // groupby - array version
    isEqual = (t.isDeeply || function (a, b, msg) {
        return t.ok(compare(a, b) == 0, msg);
    });
    isEqual(groupby_as_array([ ]    ), [                        ], "empty");
    isEqual(groupby_as_array([1,1,1]), [ [1,[1,1,1]]            ], "[1,1,1]: [1,1,1]");
    isEqual(groupby_as_array([1,2,2]), [ [1,[1]    ], [2,[2,2]] ], "[1,2,2]: [1], [2,2]");
    isEqual(groupby_as_array([1,1,2]), [ [1,[1,1]  ], [2,[2  ]] ], "[1,1,2]: [1,1], [2]");
    isEqual(groupby_as_array([null,null] ), [ [null,[null,null]] ], "[null,null]: [null,null]");
    grouped = groupby_as_array([1,1,3,2,4,6,8], odd);
    isEqual(grouped, [[1, [1,1,3]], [0,[2,4,6,8]]], "[1,1,3,2,4,6,7] odd: [1,1,3], [2,4,6,8]");
};
