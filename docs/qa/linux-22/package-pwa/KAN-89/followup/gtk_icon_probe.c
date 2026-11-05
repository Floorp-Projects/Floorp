/* Standalone GTK theme probe only: never loaded into the product. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 void *h=dlopen("libgtk-3.so.0",RTLD_NOW|RTLD_GLOBAL);if(!h){puts(dlerror());return 2;}
 int(*init)(int*,char***)=dlsym(h,"gtk_init_check");
 void*(*settings)(void)=dlsym(h,"gtk_settings_get_default");
 void*(*theme)(void)=dlsym(h,"gtk_icon_theme_get_default");
 void(*get)(void*,const char*,...)=dlsym(h,"g_object_get");
 int*(*sizes)(void*,const char*)=dlsym(h,"gtk_icon_theme_get_icon_sizes");
 void(*paths)(void*,char***,int*)=dlsym(h,"gtk_icon_theme_get_search_path");
 if(!init(0,0)){puts("gtk_init_check=false");return 3;}
 char *name=0;get(settings(),"gtk-icon-theme-name",&name,(void*)0);printf("theme=%s\n",name?name:"NULL");
 char **p=0;int n=0;paths(theme(),&p,&n);for(int i=0;i<n;i++)printf("search=%s\n",p[i]);
 const char *names[]={"floorp","firefox","one.ablaze.floorp"};
 for(int i=0;i<3;i++){int *s=sizes(theme(),names[i]);printf("icon=%s sizes=",names[i]);for(int j=0;s&&s[j]&&j<50;j++)printf("%d,",s[j]);puts("");}
 return 0;
}
