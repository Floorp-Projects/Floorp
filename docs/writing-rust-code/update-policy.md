# Rust Update Policy

We document here the decision making and planning around when we update the
Rust toolchains used and required to build Firefox.

This allows contributors to know when new features will be usable, and
downstream packagers to know what toolchain will be required for each Firefox
release. Both benefit from the predictability of a schedule. 

## Schedule

Here are the Rust versions for each Firefox version.

- The "Uses" column indicates the version of Rust used to build
  releases shipped to users.

- The "Requires" column indicates the oldest version of Rust that can
  successfully build the sources.
  
| Firefox Version | Uses | Requires | Rust "Uses" release date | Nightly Soft Freeze | Firefox release date |
|-----------------|------|----------|--------------------------|---------------------|----------------------|
| Firefox 56 | Rust 1.19.0 | 1.17.0 | 2017 April 27 | | 2017 September 26
| Firefox 57 | Rust 1.19.0 | 1.19.0 | 2017 July 20 | | 2017 November 14
| Firefox 58 | Rust 1.21.0 | 1.21.0 | 2017 October 12 | | 2018 January 16
| Firefox 59 | Rust 1.22.1 | 1.22.1 | 2017 November 23 | | 2018 March 13
| Firefox 60 | Rust 1.24.0 | 1.24.0 | 2018 February 15 | | 2018 May 9
| Firefox 61 | Rust 1.24.0 | 1.24.0 | 2018 February 15 | | 2018 June 26
| Firefox 62 | Rust 1.24.0 | 1.24.0 | 2018 February 15 | | 2018 September 5
| Firefox 63 | Rust 1.28.0 | 1.28.0 | 2018 August 2 | | 2018 October 23
| Firefox 64 | Rust 1.29.2 | 1.29.0 | 2018 September 13 | 2018 October 15 | 2018 December 11
| Firefox 65 | Rust 1.30.0 | 1.30.0 | 2018 October 25 | 2018 December 3 | 2019 January 29
| Firefox 66 | Rust 1.31.0 | 1.31.0 | 2018 December 6 | 2019 January 21 | 2019 March 19
| Firefox 67 | Rust 1.32.0 | 1.32.0 | 2019 January 17 | 2019 March 11 | 2019 May 21
| Firefox 68 | Rust 1.34.0 | 1.34.0 | 2019 April 11 | 2019 May 13 | 2019 July 9
| Firefox 69 | Rust 1.35.0 | 1.35.0 | 2019 May 23 | 2019 July 1 | 2019 September 3
| Firefox 70 | Rust 1.37.0 | 1.36.0 | 2019 July 4 | 2019 August 26 | 2019 October 22
| Firefox 71 | Rust 1.37.0 | 1.37.0 | 2019 August 15 | 2019 October 14 | 2019 December 3
| Firefox 72 | Rust 1.38.0 | 1.37.0 | 2019 August 15 | 2019 November 25 | 2020 January 7
| Firefox 73 | Rust 1.39.0 | 1.39.0 | 2019 November 7 | 2020 January 1 | 2020 February 11
| Firefox 74 | Rust 1.39.0 | 1.39.0 | 2019 November 7 | 2020 February 6 | 2020 March 10
| Firefox 75 | Rust 1.41.0 | 1.41.0 | 2020 January 30 | 2020 March 5 | 2020 April 7
| Firefox 76 | Rust 1.41.0 | 1.41.0 | 2020 January 30 | 2020 April 2 | 2020 May 5
| Firefox 77 | Rust 1.41.1 | 1.41.0 | 2020 January 30 | 2020 April 30 | 2020 June 2
| Firefox 78 | Rust 1.43.0 | 1.41.0 | 2020 April 23 | 2020 May 28 | 2020 June 30
| Firefox 79 | Rust 1.43.0 | 1.43.0 | 2020 April 23 | 2020 June 26 | 2020 July 28
| Firefox 80 | Rust 1.43.0 | 1.43.0 | 2020 April 23 | 2020 July 23 | 2020 August 25
| Firefox 81 | Rust 1.43.0 | 1.43.0 | 2020 April 23 | 2020 August 20 | 2020 September 22
| Firefox 82 | Rust 1.43.0 | 1.43.0 | 2020 April 23 | 2020 September 17 | 2020 October 20
| Firefox 83 | Rust 1.43.0 | 1.43.0 | 2020 April 23 | 2020 October 15 | 2020 November 17
| Firefox 84 | Rust 1.47.0 | 1.43.0 | 2020 October 8 | 2020 November 12 | 2020 December 15
| Firefox 85 | Rust 1.48.0 | 1.47.0 | 2020 November 19 | 2020 December 10 | 2021 January 26
| Firefox 86 | Rust 1.49.0 | 1.47.0 | 2020 December 31 | 2021 January 21 | 2021 February 23
| Firefox 87 | Rust 1.50.0 | 1.47.0 | 2021 February 11 | 2021 February 18 | 2021 March 23
| Firefox 88 | Rust 1.50.0 | 1.47.0 | 2021 February 11 | 2021 March 18 | 2021 April 19
| Firefox 89 | Rust 1.51.0 | 1.47.0 | 2021 March 25 | 2021 April 15 | 2021 June 1
| Firefox 90 | Rust 1.52.0 | 1.47.0 | 2021 May 6 | 2021 May 27 | 2021 June 29
| Firefox 91 | Rust 1.53.0 | 1.51.0 | 2021 June 17 | 2021 July 8 | 2021 August 10
| Firefox 92 | Rust 1.54.0 | 1.51.0 | 2021 July 29 | 2021 August 5 | 2021 September 7
| Firefox 93 | Rust 1.54.0 | 1.51.0 | 2021 July 29 | 2021 September 2 | 2021 October 5
| Firefox 94 | Rust 1.55.0 | 1.53.0 | 2021 September 9 | 2021 September 30 | 2021 November 2
| Firefox 95 | Rust 1.56.0 | 1.53.0 | 2021 October 21 | 2021 October 28 | 2021 December 7
| Firefox 96 | Rust 1.57.0 | 1.53.0 | 2021 December 2 | 2021 December 2 | 2022 January 11
| Firefox 97 | Rust 1.57.0 | 1.57.0 | 2021 December 2 | 2022 January 6 | 2022 February 8
| Firefox 98 | Rust 1.58.0 | 1.57.0 | 2022 January 13 | 2022 February 2 | 2022 March 8
| Firefox 99 | Rust 1.59.0 | 1.57.0 | 2022 February 24 | 2022 March 3 | 2022 April 5
| Firefox 100 | Rust 1.59.0 | 1.57.0 | 2022 February 24 | 2022 March 31 | 2022 May 3
| Firefox 101 | Rust 1.60.0 | 1.59.0 | 2022 April 7 | 2022 April 28 | 2022 May 31
| Firefox 102 | Rust 1.60.0 | 1.59.0 | 2022 April 7 | 2022 May 26 | 2022 June 28
| Firefox 103 | Rust 1.61.0 | 1.59.0 | 2022 May 19 | 2022 June 23 | 2022 July 27
| **Estimated** |
| Firefox 104 | Rust 1.62.0 | ? | 2022 June 30 | 2022 July 21 | 2022 August 23
| Firefox 105 | Rust 1.63.0 | ? | 2022 August 11 | 2022 August 18 | 2022 September 20
| Firefox 106 | Rust 1.63.0 | ? | 2022 August 11 | 2022 September 15 | 2022 October 18
| Firefox 107 | Rust 1.64.0 | ? | 2022 September 22 | 2022 October 13 | 2022 November 15

New feature adoption schedule:

| Mozilla-central can use | Starting on (Rust release + 2 weeks) |
|-------------------------|--------------------------------------|
| Rust 1.19.0 | 2017 August 3
| Rust 1.20.0 | 2017 October 17 ([was](https://bugzilla.mozilla.org/show_bug.cgi?id=1396884) September 14)
| Rust 1.21.0 | 2017 October 26
| Rust 1.22.0 | 2017 December 7
| Rust 1.23.0 | 2018 January 26 ([was](https://bugzilla.mozilla.org/show_bug.cgi?id=1418081#c8) January 18)
| Rust 1.24.0 | 2018 March 1
| Rust 1.25.0 | 2018 April 12
| Rust 1.26.0 | 2018 May 24
| Rust 1.27.0 | 2018 July 5
| Rust 1.28.0 | 2018 August 16
| Rust 1.29.0 | 2018 September 27
| Rust 1.30.0 | 2018 November 8

## Policy

### Official builds

_We ship official stable Firefox with stable Rust._

Typically we build Firefox Nightly with the current stable Rust and let that
configuration ride through beta to release. There have been frequent exceptions
where we've adopted a beta Rust during the Nightly phase to address some
platform support issue, and later updated when that Rust version became stable.
For example, we needed to use beta Rust for our own builds when we first added
Rust support to Firefox for Android.

The policy provides good stability guarantees once Firefox is in release, while
giving us the freedom to respond to the issues experience shows we'll need to
address during development.

###  Developer builds

_Local developer builds use whatever Rust toolchain is available on the
system._

Someone building Firefox can maintain the latest stable Rust with the `rustup`
for `mach bootstrap` tools, or try other variations.

### Required versions

_We require a new stable Rust starting 2 weeks after it is released._

This policy effectively removes the difference between the required Rust
version and the default target for official builds, while still giving tier-3
developers some time to process toolchain updates.

Downstream packagers still have 2-3 months to package each stable Rust release
before it’s required by a new Firefox release.

The stable+two weeks requirement is a rough target. We expect to hit it within
a few days for most Rust releases. But, for example, when the target date falls
within a mozilla-central soft freeze (before branching for a beta release) we
may update a week later.

We expect esr releases will stay on the same minimum Rust version, so
backporting security fixes may require Rust compatibility work too.

### Rationale

We need to be fast and efficient across the whole system which includes the
Rust, Servo, and Firefox development teams. We realise that the decisions we
make have an impact downstream, and we want to minimise the negative aspects of
that impact. However, we cannot do so at the expense of our own productivity.

There are many other stakeholders, of course, but our work in Gecko and Servo
are major drivers for Rust language, compiler, and library development. That
influence is more effective if we can iterate quickly. Servo itself often uses
experimental language features to give the necessary early feedback on features
as they evolve.

Rust updates every six weeks, just like Firefox. This is more like web
languages than native languages, but it's been great for building community. We
(as Gecko developers) should embrace it.

Many of us think of the toolchain and source code as being conceptually
different. At this point in time, Rust is evolving much more quickly than
Python or C++. For now it is easier to think of Rust as being part of Firefox,
rather than thinking of it like a C++ toolchain with decades of history which
we we have little influence on.

Official build configurations are generally part of the Firefox code itself,
subject to normal review, and don't affect anyone else. Therefore it's
straightforward to manage the Rust toolchain we use directly like any other
change.

For contributors, we maintain a minimum supported Rust toolchain This helps
those working on tier-3 platforms, where updating Rust can be more difficult
than just running `rustup update`, by giving them time to adapt. However, it
means other contributors must backport newer work to maintain compatibility. As
more Rust code was merged into Firefox this became expensive.

Historically we bumped this minimum every 3-4 Rust releases, which also helped
contributors on slow network connections since they didn't have to download
toolchains as often. Deciding to bump this way involved negotiating each
change, which by late 2017 many contributors felt was a more significant
burden. Delaying to give tier-3 platforms months instead of weeks to update
their Rust packages is also not considered a good trade-off.

My experience is that it takes about a week for version bumps to start pulling
in dependent crates using new language features, so updating after a couple of
weeks acts as only a moderate restraint on Servo developers working on Gecko
modules. I think this is the correct trade-off between those two groups.

### Alternate Proposals

_We require nightly Rust_

This would certainly speed up the feedback with language design. However, we
would frequently encounter build breakage when unstable language features
change. Even Servo pins to a specific nightly to avoid this problem. That’s
appropriate for a smaller research project, but not something the size of
Firefox.

We should and do test building Firefox with nightly Rust to catch compiler
issues, but that’s different from requiring everyone to use unstable Rust.

_We require beta Rust_

I don’t think anyone has suggested this, but I thought I’d include it for
completeness. This shortens the feedback cycle for new features at the expense
of more churn for contributors. It’s not a good trade-off because by the time a
new feature is stabilized in beta Rust, it’s too late to change it much. This
is better served by particular developers working with upstream directly. I
believe beta is also something of an “excluded middle” in the Rust ecosystem,
with most contributors working with either stable or unstable nightly Rust.

_We require the previous stable Rust once a new stable Rust is released._

That is, we bump the Firefox minimum to Rust 1.n.m when rust 1.(n+1).0 stable
is released. This is roughly every 6 weeks, with six weeks to update before
bumping into the new requirement. This policy might be simpler for contributors
to remember. There's not much difference in the amount of time downstream
packager receive for a particular Firefox release. This is a trade-off of more
time for tier-3 developers against people working on Gecko/Servo components
doing extra work to maintain compatibility. As I say above, I think this is the
wrong choice.

_We bump the minimum Rust version only when new features are compelling._

This reduces churn for developers, but the need to negotiate each bump takes
time better spent on development. See the motivation section above. 
