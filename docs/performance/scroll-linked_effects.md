# Scroll-linked effects

The definition of a scroll-linked effect is an effect implemented on a
webpage where something changes based on the scroll position, for
example updating a positioning property with the aim of producing a
parallax scrolling effect. This article discusses scroll-linked effects,
their effect on performance, related tools, and possible mitigation
techniques.

## Scrolling effects explained

Often scrolling effects are implemented by listening for the `scroll`
event and then updating elements on the page in some way (usually the
CSS
[`position`]((https://developer.mozilla.org/en-US/docs/Web/CSS/Position "The position CSS property sets how an element is positioned in a document. The top, right, bottom, and left properties determine the final location of positioned elements.")
or
[`transform`](https://developer.mozilla.org/en-US/docs/Web/CSS/transform "The transform CSS property lets you rotate, scale, skew, or translate an element. It modifies the coordinate space of the CSS visual formatting model.")
property.) You can find a sampling of such effects at [CSS Scroll API:
Use
Cases](https://github.com/RByers/css-houdini-drafts/blob/master/css-scroll-api/UseCases.md).

These effects work well in browsers where the scrolling is done
synchronously on the browser's main thread. However, most browsers now
support some sort of asynchronous scrolling in order to provide a
consistent 60 frames per second experience to the user. In the
asynchronous scrolling model, the visual scroll position is updated in
the compositor thread and is visible to the user before the `scroll`
event is updated in the DOM and fired on the main thread. This means
that the effects implemented will lag a little bit behind what the user
sees the scroll position to be. This can cause the effect to be laggy,
janky, or jittery --- in short, something we want to avoid.

Below are a couple of examples of effects that would not work well with
asynchronous scrolling, along with equivalent versions that would work
well:

### Example 1: Sticky positioning

Here is an implementation of a sticky-positioning effect, where the
\"toolbar\" div will stick to the top of the screen as you scroll down.

```html
<body style="height: 5000px" onscroll="document.getElementById('toolbar').style.top = Math.max(100, window.scrollY) + 'px'">
 <div id="toolbar" style="position: absolute; top: 100px; width: 100px; height: 20px; background-color: green"></div>
</body>
```

This implementation of sticky positioning relies on the scroll event
listener to reposition the "toolbar" div. As the scroll event listener
runs in the JavaScript on the browser's main thread, it will be
asynchronous relative to the user-visible scrolling. Therefore, with
asynchronous scrolling, the event handler will be delayed relative to
the user-visible scroll, and so the div will not stay visually fixed as
intended. Instead, it will move with the user's scrolling, and then
\"snap\" back into position when the scroll event handler runs. This
constant moving and snapping will result in a jittery visual effect. One
way to implement this without the scroll event listener is to use the
CSS property designed for this purpose:

```html
<body style="height: 5000px">
 <div id="toolbar" style="position: sticky; top: 0px; margin-top: 100px; width: 100px; height: 20px; background-color: green"></div>
</body>
```

This version works well with asynchronous scrolling because position of
the \"toolbar\" div is updated by the browser as the user scrolls.

### Example 2: Scroll snapping

Below is an implementation of scroll snapping, where the scroll position
snaps to a particular destination when the user's scrolling stops near
that destination.

```html
<body style="height: 5000px">
 <script>
    function snap(destination) {
        if (Math.abs(destination - window.scrollY) < 3) {
            scrollTo(window.scrollX, destination);
        } else if (Math.abs(destination - window.scrollY) < 200) {
            scrollTo(window.scrollX, window.scrollY + ((destination - window.scrollY) / 2));
            setTimeout(snap, 20, destination);
        }
    }
    var timeoutId = null;
    addEventListener("scroll", function() {
        if (timeoutId) clearTimeout(timeoutId);
        timeoutId = setTimeout(snap, 200, parseInt(document.getElementById('snaptarget').style.top));
    }, true);
 </script>
 <div id="snaptarget" class="snaptarget" style="position: relative; top: 200px; width: 100%; height: 200px; background-color: green"></div>
</body>
```

In this example, there is a scroll event listener which detects if the
scroll position is within 200 pixels of the top of the \"snaptarget\"
div. If it is, then it triggers an animation to \"snap\" the scroll
position to the top of the div. As this animation is driven by
JavaScript on the browser's main thread, it can be interrupted by other
JavaScript running in other tabs or other windows. Therefore, the
animation can end up looking janky and not as smooth as intended.
Instead, using the CSS snap-points property will allow the browser to
run the animation asynchronously, providing a smooth visual effect to
the user.

```html
<body style="height: 5000px">
 <style>
    body, /* blink currently has bug that requires declaration on `body` */
    html {
      scroll-snap-type: y proximity;
    }
    .snaptarget {
      scroll-snap-align: start;
      position: relative;
      top: 200px;
      height: 200px;
      background-color: green;
    }
 </style>
 <div class="snaptarget"></div>
</body>
```

This version can work smoothly in the browser even if there is
slow-running Javascript on the browser's main thread.

### Other effects

In many cases, scroll-linked effects can be reimplemented using CSS and
made to run on the compositor thread. However, in some cases the current
APIs offered by the browser do not allow this. In all cases, however,
Firefox will display a warning to the developer console (starting in
version 46) if it detects the presence of a scroll-linked effect on a
page. Pages that use scrolling effects without listening for scroll
events in JavaScript will not get this warning. See the [Asynchronous
scrolling in Firefox](https://staktrace.com/spout/entry.php?id=834) blog
post for some more examples of effects that can be implemented using CSS
to avoid jank.

## Future improvements

Going forward, we would like to support more effects in the compositor.
In order to do so, we need you (yes, you!) to tell us more about the
kinds of scroll-linked effects you are trying to implement, so that we
can find good ways to support them in the compositor. Currently there
are a few proposals for APIs that would allow such effects, and they all
have their advantages and disadvantages. The proposals currently under
consideration are:

-   [Web Animations](https://w3c.github.io/web-animations/): A new API
    for precisely controlling web animations in JavaScript, with an
    [additional
    proposal](https://wiki.mozilla.org/Platform/Layout/Extended_Timelines)
    to map scroll position to time and use that as a timeline for the
    animation.
-   [CompositorWorker](https://docs.google.com/document/d/18GGuTRGnafai17PDWjCHHAvFRsCfYUDYsi720sVPkws/edit?pli=1#heading=h.iy9r1phg1ux4):
    Allows JavaScript to be run on the compositor thread in small
    chunks, provided it doesn't cause the framerate to drop.
-   [Scroll
    Customization](https://docs.google.com/document/d/1VnvAqeWFG9JFZfgG5evBqrLGDZYRE5w6G5jEDORekPY/edit?pli=1):
    Introduces a new API for content to dictate how a scroll delta is
    applied and consumed. As of this writing, Mozilla does not plan to
    support this proposal, but it is included for completeness.

### Call to action

If you have thoughts or opinions on:

-   Any of the above proposals in the context of scroll-linked effects.
-   Scroll-linked effects you are trying to implement.
-   Any other related issues or ideas.

Please get in touch with us! You can join the discussion on the
[public-houdini](https://lists.w3.org/Archives/Public/public-houdini/)
mailing list.
