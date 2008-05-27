#ifndef mozce_map_h
#define mozce_map_h

extern "C" {
#if 0
}
#endif
struct mapping{
char* key;
char* value;
mapping* next;
};


int map_put(const char* key,const char* val);
char*  map_get(const char* key);

static int init_i =1;
static mapping initial_map[] = {
#ifdef DEBUG_NSPR_ALL
    {"NSPR_LOG_MODULES", "all:5",initial_map + (init_i++)},
    {"NSPR_LOG_FILE","nspr.log",initial_map + (init_i++)},
#endif  
#ifdef TIMELINE
    {"NS_TIMELINE_LOG_FILE","\\bin\\timeline.log",initial_map + (init_i++)},
    {"NS_TIMELINE_ENABLE", "1",initial_map + (init_i++)},
#endif
    {"tmp", "/Temp",initial_map + (init_i++)},
    {"GRE_HOME",".",initial_map + (init_i++)},
	{ "NSPR_FD_CACHE_SIZE_LOW", "10",initial_map + (init_i++)},              
    {"NSPR_FD_CACHE_SIZE_HIGH", "30",initial_map + (init_i++)},
    {"XRE_PROFILE_PATH", "\\Application Data\\Mozilla\\Profiles",initial_map + (init_i++)},
    {"XRE_PROFILE_LOCAL_PATH","./profile",initial_map + (init_i++)},
    {"XRE_PROFILE_NAME","default",0}
};

static mapping* head = initial_map;


#if 0
{
#endif
} /* extern "C" */

#endif