
function test_matchObject()
{
    var f = true;
    
    obj1 = {foo:"hey", bar:"ho"}
    obj2 = {a:"1", b:"2"}

    p1 = {foo:"hey"}
    p2 = {bar:"ho"}
    p3 = {a:"1"}
    p4 = {b:"2"}

    /* true, single pattern, and it matches */
    f &= matchObject (obj1, p1);
    /* false, single pattern matches, negated */
    f &= !matchObject (obj1, p1, true);

    /* false, single pattern doesnt match*/
    f &= !matchObject (obj1, p3);
    /* true, single pattern doesnt match, negated */
    f &= matchObject (obj1, p3, true);    

    /* true, p1 matches */
    f &= matchObject (obj1, [p1, p3]);
    /* false, p1 matches, negated */
    f &= !matchObject (obj1, [p1, p3], true);
    
    /* true, both paterns match */
    f &= matchObject (obj2, [p3, p4]);
    /* false, both patterns match, negated */
    f &= !matchObject (obj2, [p3, p4], true);
    
    /* false, neither pattern matches */
    f &= !matchObject (obj1, [p3, p4]);
    /* true, neither pattern matches, negated */
    f &= matchObject (obj1, [p3, p4], true);

    return Boolean(f);  /* you've got to find any problems by hand :) */
   
}