// |jit-test| allow-oom;
function startTest() {
 if (typeof document != "object" 
    || !document.location.href.match(/jsreftest.html/))  {}
};
gczeal(4);
startTest();
ArrayBuffer( 8192 );
