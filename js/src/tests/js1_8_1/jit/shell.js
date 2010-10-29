// The loop count at which we trace
const RECORDLOOP = this.tracemonkey ? tracemonkey.HOTLOOP : 8;
// The loop count at which we run the trace
const RUNLOOP = RECORDLOOP + 1;
