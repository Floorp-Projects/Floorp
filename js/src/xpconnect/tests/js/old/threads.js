
const LOOP_COUNT   = 10000;
const THREAD_COUNT = 10;

const nsIThread = Components.interfaces.nsIThread;
const ThreadContractID = "@mozilla.org/thread;1";
const Thread = new Components.Constructor(ThreadContractID, "nsIThread", "init");

var foo;
var bar;

function threadProc() {
    foo = this.id+1;
    bar[this.id] = {p : 0};
    var n, m;
    for(n in bar) {
        // Uncommenting the line below makes this a lot more likely to deadlock.
        for(m in bar[n]) {}
    } 
    // Commented this because I was thinking that the printf impl being called 
    // on so many thread might be contributing to some apparent memory 
    // corruption cases.
    // print("  printed from other thread "+this.id+". foo is: "+foo);
    for(n in this.__parent__) /* print(n) */;
}

function runThreads() {
    print("  printed from main thread. foo is "+foo);

    var threads = new Array(THREAD_COUNT);
    var i;

    for(i = 0; i < THREAD_COUNT; i++) {
        var runable = {run : threadProc, id : i};
        threads[i] = new Thread(runable, 0, 
                                nsIThread.PRIORITY_NORMAL,
                                nsIThread.SCOPE_GLOBAL,
                                nsIThread.STATE_JOINABLE);
    }

    print("  printed from main thread. foo is "+foo);

    // var main_thread = threads[0].currentThread;

    for(i = THREAD_COUNT-1; i >= 0; i--) {
        // main_thread.sleep(1);
        threads[i].join();
    }

    print("  printed from main thread. foo is "+foo);
}

var total_interval = 0;

for(i = 0; i < LOOP_COUNT; i++) {
    var start_time = new Date().getTime()/1000;

    foo = 0;
    bar = new Array(THREAD_COUNT);
    print("--- loop number "+i);
    runThreads();

    var end_time = new Date().getTime()/1000;

    var interval = parseInt(100*(end_time - start_time),10)/100;
    print("        Interval = "+interval+ " seconds.");
    // ignore first loop
    if(i) {
        total_interval += end_time - start_time;
        var average_interval = parseInt(100*(total_interval / i),10)/100
        print("Average Interval = "+average_interval+" seconds.");
    }
}    

