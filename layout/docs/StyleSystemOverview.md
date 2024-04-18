# Style System Overview

## Quantum CSS (Stylo)

Starting with Firefox 57 and later, Gecko makes use of the parallel
style system written in Rust that comes from Servo. There\'s an
[overview](https://hacks.mozilla.org/2017/08/inside-a-super-fast-css-engine-quantum-css-aka-stylo/)
of this with graphics to help explain what\'s going on. The [Servo
wiki](https://github.com/servo/servo/wiki/Layout-Overview) has some more
details.

## Gecko

The rest of the style section section describes the Gecko style system
used in Firefox 56 and earlier. Some bits may still apply, but it likely
needs revising.

In order to display the content, Gecko needs to compute the styles
relevant to each DOM node. It does this based on the model described in
the CSS specifications: this model applies to style specified in CSS
(e.g. by a \'style\' element, an \'xml-stylesheet\' processing
instruction or a \'style\' attribute), style specified by presentation
attributes, and the default style specified by our own user agent style
sheets. There are two major sets of data structures within the style
system:

-   first, data structures that represent sources of style data, such as
    CSS style sheets or data from stylistic HTML attributes
-   second, data structures that represent computed style for a given
    DOM node.

These sets of data structures are mostly distinct (for example, they
store values in different ways).

The loading of CSS style sheets from the network is managed by the [CSS
loader](https://dxr.mozilla.org/mozilla-central/source/layout/style/Loader.h);
they are then tokenized by the [CSS
scanner](https://dxr.mozilla.org/mozilla-central/source/layout/style/nsCSSScanner.h)
and parsed by the [CSS
parser](https://dxr.mozilla.org/mozilla-central/source/layout/style/nsCSSParser.h).
Those that are attached to the document also expose APIs to script that
are known as the CSS Object Model, or CSSOM.

The style sheets that apply to a document are managed by a class called
the [style
set](https://dxr.mozilla.org/mozilla-central/source/layout/style/nsStyleSet.h).
The style set interacts with the different types of style sheets
(representing CSS style sheets, presentational attributes, and \'style\'
attributes) through two interfaces:
[nsIStyleSheet](http://dxr.mozilla.org/mozilla-central/source/layout/style/nsIStyleSheet.h)
for basic management of style sheets and
[nsIStyleRuleProcessor](http://dxr.mozilla.org/mozilla-central/source/layout/style/nsIStyleRuleProcessor.h)
for getting the style data out of them. Usually the same object
implements both interfaces, except in the most important case, CSS style
sheets, where there is a single rule processor for all of the CSS style
sheets in each origin (user/UA/author) of the CSS cascade.

The computed style data for an element/frame are exposed to the rest of
Gecko through the class mozilla::ComputedStyle (previously called
nsStyleContext). Rather than having a member variable for each CSS
property, it breaks up the properties into groups of related properties
called style structs. These style structs obey the rule that all of the
properties in a single struct either inherit by default (what the CSS
specifications call \"Inherited: yes\" in the definition of properties;
we call these inherited structs) or all are not inherited by default (we
call these reset structs). Separating the properties in this way
improves the ability to share the structs between similar ComputedStyle
objects and reduce the amount of memory needed to store the style data.
The ComputedStyle API exposes a method for getting each struct, so
you\'ll see code like `sc->GetStyleText()->mTextAlign` for getting the
value of the text-align CSS property. (Frames (see the Layout section
below) also have the same GetStyle\* methods, which just forward the
call to the frame\'s ComputedStyle.)

The ComputedStyles form a tree structure, in a shape somewhat like the
content tree (except that we coalesce identical sibling ComputedStyles
rather than keeping two of them around; if the parents have been
coalesced then this can apply recursively and coalasce cousins, etc.; we
do not coalesce parent/child ComputedStyles). The parent of a
ComputedStyle has the style data that the ComputedStyle inherits from
when CSS inheritance occurs. This means that the parent of the
ComputedStyle for a DOM element is generally the ComputedStyle for that
DOM element\'s parent, since that\'s how CSS says inheritance works.

The process of turning the style sheets into computed style data goes
through three main steps, the first two of which closely relate to the
[nsIStyleRule](http://dxr.mozilla.org/mozilla-central/source/layout/style/nsIStyleRule.h)
interface, which represents an immutable source of style data,
conceptually representing (and for CSS style rules, directly storing) a
set of property:value pairs. (It is similar to the idea of a CSS style
rule, except that it is immutable; this immutability allows for
significant optimization. When a CSS style rule is changed through
script, we create a new style rule.)

The first step of going from style sheets to computed style data is
finding the ordered sequence of style rules that apply to an element.
The order represents which rules override which other rules: if two
rules have a value for the same property, the higher ranking one wins.
(Note that there\'s another difference from CSS style rules:
declarations with !important are represented using a separate style
rule.) This is done by calling one of the
nsIStyleRuleProcessor::RulesMatching methods. The ordered sequence is
stored in a [trie](http://en.wikipedia.org/wiki/Trie) called the rule
tree: the path from the root of the rule tree to any (leaf or non-leaf)
node in the rule tree represents a sequence of rules, with the highest
ranking farthest from the root. Each rule node (except for the root) has
a pointer to a rule, but since a rule may appear in many sequences,
there are sometimes many rule nodes pointing to the same rule. Once we
have this list we create a ComputedStyle (or find an appropriate
existing sibling) with the correct parent pointer (for inheritance) and
rule node pointer (for the list of rules), and a few other pieces of
information (like the pseudo-element).

The second step of going from style sheets to computed style data is
getting the winning property:value pairs from the rules. (This only
provides property:value pairs for some of the properties; the remaining
properties will fall back to inheritance or to their initial values
depending on whether the property is inherited by default.) We do this
step (and the third) for each style struct, the first time it is needed.
This is done in nsRuleNode::WalkRuleTree, where we ask each style rule
to fill in its property:value pairs by calling its MapRuleInfoInto
function. When called, the rule fills in only those pairs that haven\'t
been filled in already, since we\'re calling from the highest priority
rule to the lowest (since in many cases this allows us to stop before
going through the whole list, or to do partial computation that just
adds to data cached higher in the rule tree).

The third step of going from style sheets to computed style data (which
various caching optimizations allow us to skip in many cases) is
actually doing the computation; this generally means we transform the
style data into the data type described in the \"Computed Value\" line
in the property\'s definition in the CSS specifications. This
transformation happens in functions called nsRuleNode::Compute\*Data,
where the \* in the middle represents the name of the style struct. This
is where the transformation from the style sheet value storage format to
the computed value storage format happens.

Once we have the computed style data, we then store it: if a style
struct in the computed style data doesn\'t depend on inherited values or
on data from other style structs, then we can cache it in the rule tree
(and then reuse it, without recomputing it, for any ComputedStyles
pointing to that rule node). Otherwise, we store it on the ComputedStyle
(in which case it may be shared with the ComputedStyle\'s descendant
ComputedStyles). This is where keeping inherited and non-inherited
properties separate is useful: in the common case of relatively few
properties being specified, we can generally cache the non-inherited
structs in the rule tree, and we can generally share the inherited
structs up and down the ComputedStyle tree.

The ownership models in style sheet structures are a mix of reference
counted structures (for things accessible from script) and directly
owned structures. ComputedStyles are reference counted, and own their
parents (from which they inherit), and rule nodes are garbage collected
with a simple mark and sweep collector (which often never needs to run).

-   code:
    [layout/style/](http://dxr.mozilla.org/mozilla-central/source/layout/style/),
    where most files have useful one line descriptions at the top that
    show up in DXR
-   Bugzilla: Style System (CSS)
-   specifications
    -   [CSS 2.1](http://www.w3.org/TR/CSS21/)
    -   [CSS 2010, listing stable css3
        modules](http://www.w3.org/TR/css-2010/)
    -   [CSS WG editors drafts](http://dev.w3.org/csswg/) (often more
        current, but sometimes more unstable than the drafts on the
        technical reports page)
    -   [Preventing attacks on a user\'s history through CSS :visited
        selectors](http://dbaron.org/mozilla/visited-privacy)
-   documentation
    -   [style system
        documentation](http://www-archive.mozilla.org/newlayout/doc/style-system.html)
        (somewhat out of date)
