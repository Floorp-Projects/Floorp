JS = $(shell find . -name '*.js' -print)

all: menu.html

menu.html: menuhead.html menufoot.html $(JS) Makefile
	exec perl mklistpage.pl > menubody.html
	cat menuhead.html menubody.html menufoot.html > menu.html

clean:
	rm -f menubody.html menu.html
