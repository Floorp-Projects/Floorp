a=1;
for (i=0; i<10; i++){
    a = inc (inc(a));
}

c = a+2;

// Increment function
function inc (b){
    return b+1;
}