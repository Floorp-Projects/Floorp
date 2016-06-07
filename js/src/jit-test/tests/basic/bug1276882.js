// |jit-test| error: sleep interval is not a number
sleep(0.001);
1;
sleep(0.1);
sleep(this);
