function outerline(){
    var i=0;
    var j=0;

    for (i = 3; i<= 100000; i+=2)
    {
        for (j = 3; j < 1000; j+=2)
        {
            if ((i & 1) == 1)
                break;
        }
    }
    return "ok";
}
assertEq(outerline(), "ok");
