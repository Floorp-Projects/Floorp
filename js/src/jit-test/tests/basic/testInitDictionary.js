
var shapes = {};

function stringify(a) {
  assertEq(shapes[shapeOf(a)], undefined);
  shapes[shapeOf(a)] = 1;
  var b = "";
  for (var c in a) {
    b += c + ":";
    if (typeof a[c] == "function")
      b += "function,";
    else
      b += a[c] + ",";
  }
  return b;
}

function test1() {
  return stringify({a: 0, b: 1, a: function() {} });
}
for (var i = 0; i < 3; i++)
  assertEq(test1(), "a:function,b:1,");

// This does not cause the object to go to dictionary mode, unlike the above.
function test2() {
  return stringify({a: 0, b: 1, a: 2, b: 3});
}
assertEq(test2(), "a:2,b:3,");

function test3() {
  return stringify({
    aa:0,ab:1,ac:2,ad:3,ae:4,af:5,ag:6,ah:7,ai:8,aj:9,
    ba:0,bb:1,bc:2,bd:3,be:4,bf:5,bg:6,bh:7,bi:8,bj:9,
    ca:0,cb:1,cc:2,cd:3,ce:4,cf:5,cg:6,ch:7,ci:8,cj:9,
    da:0,db:1,dc:2,dd:3,de:4,df:5,dg:6,dh:7,di:8,dj:9,
    ea:0,eb:1,ec:2,ed:3,ee:4,ef:5,eg:6,eh:7,ei:8,ej:9,
    fa:0,fb:1,fc:2,fd:3,fe:4,ff:5,fg:6,fh:7,fi:8,fj:9,
    ga:0,gb:1,gc:2,gd:3,ge:4,gf:5,gg:6,gh:7,gi:8,gj:9,
    ha:0,hb:1,hc:2,hd:3,he:4,hf:5,hg:6,hh:7,hi:8,hj:9,
    ia:0,ib:1,ic:2,id:3,ie:4,if:5,ig:6,ih:7,ii:8,ij:9,
    ja:0,jb:1,jc:2,jd:3,je:4,jf:5,jg:6,jh:7,ji:8,jj:9,
    ka:0,kb:1,kc:2,kd:3,ke:4,kf:5,kg:6,kh:7,ki:8,kj:9,
    la:0,lb:1,lc:2,ld:3,le:4,lf:5,lg:6,lh:7,li:8,lj:9,
    ma:0,mb:1,mc:2,md:3,me:4,mf:5,mg:6,mh:7,mi:8,mj:9,
    na:0,nb:1,nc:2,nd:3,ne:4,nf:5,ng:6,nh:7,ni:8,nj:9,
    oa:0,ob:1,oc:2,od:3,oe:4,of:5,og:6,oh:7,oi:8,oj:9,
    pa:0,pb:1,pc:2,pd:3,pe:4,pf:5,pg:6,ph:7,pi:8,pj:9,
        });
}
for (var i = 0; i < HOTLOOP + 2; i++)
  assertEq(test3(), "aa:0,ab:1,ac:2,ad:3,ae:4,af:5,ag:6,ah:7,ai:8,aj:9,ba:0,bb:1,bc:2,bd:3,be:4,bf:5,bg:6,bh:7,bi:8,bj:9,ca:0,cb:1,cc:2,cd:3,ce:4,cf:5,cg:6,ch:7,ci:8,cj:9,da:0,db:1,dc:2,dd:3,de:4,df:5,dg:6,dh:7,di:8,dj:9,ea:0,eb:1,ec:2,ed:3,ee:4,ef:5,eg:6,eh:7,ei:8,ej:9,fa:0,fb:1,fc:2,fd:3,fe:4,ff:5,fg:6,fh:7,fi:8,fj:9,ga:0,gb:1,gc:2,gd:3,ge:4,gf:5,gg:6,gh:7,gi:8,gj:9,ha:0,hb:1,hc:2,hd:3,he:4,hf:5,hg:6,hh:7,hi:8,hj:9,ia:0,ib:1,ic:2,id:3,ie:4,if:5,ig:6,ih:7,ii:8,ij:9,ja:0,jb:1,jc:2,jd:3,je:4,jf:5,jg:6,jh:7,ji:8,jj:9,ka:0,kb:1,kc:2,kd:3,ke:4,kf:5,kg:6,kh:7,ki:8,kj:9,la:0,lb:1,lc:2,ld:3,le:4,lf:5,lg:6,lh:7,li:8,lj:9,ma:0,mb:1,mc:2,md:3,me:4,mf:5,mg:6,mh:7,mi:8,mj:9,na:0,nb:1,nc:2,nd:3,ne:4,nf:5,ng:6,nh:7,ni:8,nj:9,oa:0,ob:1,oc:2,od:3,oe:4,of:5,og:6,oh:7,oi:8,oj:9,pa:0,pb:1,pc:2,pd:3,pe:4,pf:5,pg:6,ph:7,pi:8,pj:9,");
