function test() {
    is(window._content, window.content,
       "_content needs to work, since extensions use it");
}
