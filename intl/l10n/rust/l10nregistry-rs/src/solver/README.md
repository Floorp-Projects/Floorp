
Source Order Problem Solver
======================

This module contains an algorithm used to power the `FluentBundle` generator in `L10nRegistry`.

The main concept behind it is a problem solver which takes a list of resources and a list of sources and computes all possible iterations of valid combinations of source orders that allow for creation of `FluentBundle` with the requested resources.

The algorithm is notoriously hard to read, write, and modify, which prompts this documentation to be extensive and provide an example with diagram presentations to aid the reader.

# Example
For the purpose of a graphical illustration of the example, we will evaluate a scenario with two sources and three resources.

The sources and resource identifiers will be named in concise way (*1* or *A*) to simplify diagrams, while a more tangible names derived from real-world examples in Firefox use-case will be listed in their initial definition.

### Sources
A source can be a packaged directory, and a language pack, or any other directory, zip file, or remote source which contains localization resource files.
In the example, we have two sources:
* Source 1 named ***0*** (e.g. `browser`)
* Source 2 named ***1*** (e.g. `toolkit`)

### Resources
A resource is a single Fluent Translation List file. `FluentBundle` is a combination of such resources used together to resolve translations. This algorithm operates on lists of resource identifiers which represent relative paths within the source.
In the example we have three resources:
* Resource 1 named ***A*** (e.g. `branding/brand.ftl`)
* Resource 2 named ***B*** (e.g. `errors/common.ftl`)
* Resource 3 named ***C*** (e.g. `menu/list.ftl`)

## Task
The task in this example is to generate all possible iterations of the three resources from the given two sources. Since I/O is expensive, and in most production scenarios all necessary translations are available in the first set, the iterator is used to lazily fallback on the alternative sets only in case of missing translations.

If all resources are available in both sources, the iterator should produce the following results:
1. `[A0, B0, C0]`
2. `[A0, B0, C1]`
3. `[A0, B1, C0]`
4. `[A0, B1, C1]`
5. `[A1, B0, C0]`
6. `[A1, B0, C1]`
7. `[A1, B1, C0]`
8. `[A1, B1, C1]`

Since the resources are defined by their column, we can store the resources as `[A, B, C]` separately and simplify the notation to just:
1. `[0, 0, 0]`
2. `[0, 0, 1]`
3. `[0, 1, 0]`
4. `[0, 1, 1]`
5. `[1, 0, 0]`
6. `[1, 0, 1]`
7. `[1, 1, 0]`
8. `[1, 1, 1]`

This notation will be used from now on.

## State

For the in-detail diagrams on the algorithm, we'll use another way to look at the iterator - by evaluating it state. At every point of the algorithm, there is a *partial solution* which may lead to a *complete solution*. It is encoded as:

```rust
struct Solution {
    candidate: Vec<usize>,
    idx: usize,
}
```

and which starting point can be visualized as:

```text
  ▼
┌┲━┱┬───┬───┐
│┃0┃│   │   │
└╂─╂┴───┴───┘
 ┃ ┃
 ┗━┛
```
###### Diagrams generated with use of http://marklodato.github.io/js-boxdrawing/

where the horizontal block is a candidate, vertical block is a set of sources possible for each resource, and the arrow represents the index of a resource the iterator is currently evaluating.

With those tools introduced, we can now guide the reader through how the algorithm works.
But before we do that, it is important to justify writing a custom algorithm in place of existing generic solutions, and explain the two testing strategies which heavily impact the algorithm.

# Existing libraries
Intuitively, the starting point to exploration of the problem scope would be to look at it as some variation of the [Cartesian Product](https://en.wikipedia.org/wiki/Cartesian_product) iterator.

#### Python

In Python, `itertools` package provides a function [`itertools::product`](https://docs.python.org/3/library/itertools.html#itertools.product) which can be used to generate such iterator:
```python
import itertools

for set in itertools.product(range(2), repeat=3):
    print(set)
```

#### Rust

In Rust, crate [`itertools`](https://crates.io/crates/itertools) provides, [`multi_cartesian_product`](https://docs.rs/itertools/0.9.0/itertools/trait.Itertools.html#method.multi_cartesian_product) which can be used like this:
```rust
use itertools::Itertools;

let multi_prod = (0..3).map(|i| 0..2)
    .multi_cartesian_product();

for set in multi_prod {
    println!("{:?}", set);
}
```
([playground](https://play.rust-lang.org/?version=stable&mode=debug&edition=2018&gist=6ef231f6b011b234babb0aa3e68b78ab))

#### Reasons for a custom algorithm

Unfortunately, the computational complexity of generating all possible sets is growing exponentially, both in the cost of CPU and memory use.
On a high-end laptop, computing the sets for all possible variations of the example above generates *8* sets and takes only *700 nanoseconds*, but computing the same for four sources and 16 resources (a scenario theoretically possible in Firefox with one language pack and Preferences UI for example) generates over *4 billion* sets and takes over *2 minutes*.

Since one part of static cost is the I/O, the application of a [Memoization](https://en.wikipedia.org/wiki/Memoization) technique allows us to minimize the cost of constructing, storing and retrieving sets.

Second important observation is that in most scenarios any resource exists in only some of the sources, and ability to bail out from a branch of candidates that cannot lead to a solution yields significantly fewer permutations in result.

## Optimizations

The algorithm used here is highly efficient. For the conservative scenario listed above, where 4 sources and 15 resources are all present in every source, the total time on the reference hardware is cut from *2 minutes* to *24 seconds*, while generating the same *4 billion* sets for a **5x** performance improvement.

### Streaming Iterator
Unline regular iterator, a streaming iterator allows a borrowed reference to be returned, which in this case, where the solver yields a read-only "view" of a solution, allows us to avoid having to clone it.

### Cache
Memory is much less of a problem for the algorithm than CPU usage, so the solver uses a matrix of source/resource `Option` to memoize visited cells. This allows for each source/resource combination to be tested only once after which all future tests can be skipped.

### Backtracking
This optimization allows to benefit from the recognition of the fact that most resources are only available in some sources.
Instead of generating all possible sets and then ignoring ones which are incomplete, it allows the algorithm to [backtrack](https://en.wikipedia.org/wiki/Backtracking) from partial candidates that cannot lead to a complete solution.

That technique is very powerful in the `L10nRegistry` use case and in many scenarios leads to 10-100x speed ups even in cases where all sets have to be generated.

# Serial vs Parallel Testing
At the core of the solver is a *tester* component which is responsible for eagerly evaluating candidates to allow for early bailouts from partial solutions which cannot lead to a complete solution.

This can be performed in one of two ways:

### Serial
 The algorithm is synchronous and each extension of the candidate is evaluated serially, one by one, allowing the for *backtracking* as soon as a given extension of a partial solution is confirmed to not lead to a complete solution.

Bringing back the initial state of the solver:

```text
  ▼
┌┲━┱┬───┬───┐
│┃0┃│   │   │
└╂─╂┴───┴───┘
 ┃ ┃
 ┗━┛
```

The tester will evaluate whether the first resource **A** is available in the first source **0**. The testing will be performed synchronously, and the result will inform the algorithm on whether the candidate may lead to a complete solution, or this branch should be bailed out from, and the next candidate must be tried.

#### Success case

If the test returns a success, the extensions of the candidate is generated:
```text
      ▼
┌┲━┱┬┲━┱┬───┐
│┃0┃│┃0┃│   │
└╂─╂┴╂─╂┴───┘
 ┃ ┃ ┃ ┃
 ┗━┛ ┗━┛
```

When a candidate is complete, in other words, when the last cell of a candidate has been tested and did not lead to a backtrack, we know that the candidate is a solution to the problem, and we can yield it from the iterator.

#### Failure case

If the test returns a failure, the next step is to evaluate alternative source for the same resource. Let's assume that *Source 0* had *Resource A* but it does not have *Resource B*. In such case, the algorithm will increment the second cell's source index:

```text
      ▼
     ┏━┓
     ┃0┃
┌┲━┱┬╂─╂┬───┐
│┃0┃│┃1┃│   │
└╂─╂┴┺━┹┴───┘
 ┃ ┃
 ┗━┛
 ```

and that will potentially lead to a partial solution `[0, 1, ]` to be stored for the next iteration.

If the test fails and no more sources can be generated, the algorithm will *backtrack* from the current cell looking for a cell with the **highest** index prior to the cell that was being evaluated which is not yet on the last source. If such cell is found, the results of all cells **to the right** of the newfound cell are **erased** and the next branch can be evaluated.

If no such cell can be found, that means that the iterator is complete.

### Parallel

If the testing can be performed in parallel, like an asynchronous I/O, the above *serial* solution is sub-optimal as it misses on the benefit of testing multiple cells at once.

In such a scenario, the algorithm will construct a candidate that *can* be valid (bailing only from candidates that have been already memoized as unavailable), and then test all of the untested cells in that candidate at once.

```text
          ▼
┌┲━┱┬┲━┱┬┲━┱┐
│┃0┃│┃0┃│┃0┃│
└╂─╂┴╂─╂┴╂─╂┘
 ┃ ┃ ┃ ┃ ┃ ┃
 ┗━┛ ┗━┛ ┗━┛
```

When the parallel execution returns, the algorithm memoizes all new cell results and tests if the candidate is now a valid complete solution.

#### Success case

If the result a set of successes, the candidate is returned as a solution, and the algorithm proceeds to the same operation as if it was a failure.

#### Failure case
If the result contains failures, the iterator will now backtrack to find the closest lower or equal cell to the current index which can be advanced to the next source.
In the example state above, the current cell can be advanced to *source 1* and then just a set of `[None, None, 1]` is to be evaluated by the tester (since we know that *A0* and *B0* are valid).

If that is successful, the `[0, 0, 1]` set is a complete solution and is yielded.

Then, if the iterator is resumed, the next state to be tested is:

```text
      ▼
     ┏━┓
     ┃0┃
┌┲━┱┬╂─╂┬┲━┱┐
│┃0┃│┃1┃│┃0┃│
└╂─╂┴┺━┹┴╂─╂┘
 ┃ ┃     ┃ ┃
 ┗━┛     ┗━┛
```

since cell *2* was at the highest index, cell *1* is the highest lower than *2* that was not at the highest source index position. That cell is advanced, and all cells after it are *pruned* (in this case, cell *2* is the only one). Then, the memoization kicks in, and since *A0* and *C0* are already cached as valid, the tester receives just `[None, 1, None]` to be tested and the algorithm continues.

# Summary

The algorithm explained above is tailored to the problem domain of `L10nRegistry` and is designed to be further extended in the future.

It is important to maintain this guide up to date as any changes to the algorithm are to be made.

Good luck.
