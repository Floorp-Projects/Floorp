function drawRect(x,y,dx,dy)
{
    MoveTo(x,y);
    LineTo(x+dx,y);
    LineTo(x+dx,y+dy);
    LineTo(x,y+dy);
    LineTo(x,y);
}    

function drawSquare(x,y,d)
{
    MoveTo(x,y);
    LineTo(x+d,y);
    LineTo(x+d,y+d);
    LineTo(x,y+d);
    LineTo(x,y);
}    
